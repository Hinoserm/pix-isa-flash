; Virtual disk is drive 80h. Other hard-disk requests are translated to the
; previous BIOS. Floppy requests chain unchanged.
int13_handler:
    cmp dl, 80h
    jb .chain
    pushf
    add word [cs:dbg_calls], 1
    adc word [cs:dbg_calls+2], 0
    mov [cs:dbg_last_ax], ax
    mov [cs:dbg_last_cx], cx
    mov [cs:dbg_last_dx], dx
    mov word [cs:dbg_phase], 0
    popf
    cmp dl, [cs:virtual_drive]
    je .ours
    cmp byte [cs:old_drive_count], 0
    je .absent
    ; An unknown drive must not become a phantom motherboard disk simply
    ; because insertion incremented the shared BIOS hard-disk count.
    push ax
    mov al, [cs:drive_count]
    add al, 80h
    cmp dl, al
    pop ax
    jae .absent
    mov word [cs:dbg_phase], 3
    call trace_native
    push bp
    mov bp, sp
    push ax
    push dx
    cmp dl, [cs:virtual_drive]
    jb .native_call
    dec dl
.native_call:
    ; Present the original disk count to the previous controller BIOS.
    push ax
    push ds
    xor ax, ax
    mov ds, ax
    mov al, [cs:old_drive_count]
    mov [475h], al
    pop ds
    pop ax
    pushf
    call far [cs:old13]
    pushf
    push ax
    push ds
    xor ax, ax
    mov ds, ax
    mov al, [cs:drive_count]
    mov [475h], al
    pop ds
    pop ax
    cmp byte [ss:bp-1], 8
    je .native_geometry
    cmp byte [ss:bp-1], 15h
    je .native_flags
    mov dl, [ss:bp-4]
    jmp .native_flags
.native_geometry:
    test word [ss:bp-6], 1
    jnz .native_flags
    mov dl, [cs:drive_count]
.native_flags:
    push ax
    mov ax, [ss:bp-6]
    and ax, 1
    and word [ss:bp+6], 0fffeh
    or [ss:bp+6], ax
    or word [ss:bp+6], 200h
    pop ax
    mov sp, bp
    pop bp
    iret
.absent:
    call trace_absent
    push bp
    mov bp, sp
    mov ax, 0100h
    or word [ss:bp+6], 201h
    pop bp
    iret
.chain:
    jmp far [cs:old13]
.ours:
    mov word [cs:dbg_phase], 1
    push bp
    mov bp, sp
    push ds
    push es
    pusha
    push cs
    pop ds
    cmp byte [io_busy], 0
    jne .busy
    mov byte [io_busy], 1
    mov [entry_bp], bp
    mov ax, ss
    mov [entry_ss], ax
    mov ax, [ss:bp-6]
    mov [request_ax], ax
    mov [request_fn], ah
    xor ah, ah
    mov [io_count], ax
    mov ax, [ss:bp-12]
    mov [r_bx], ax
    mov ax, [ss:bp-8]
    mov [r_cx], ax
    mov ax, [ss:bp-10]
    mov [r_dx], ax
    mov ax, [ss:bp-4]
    mov [r_es], ax
    mov ax, [ss:bp-18]
    mov [r_si], ax
    mov ax, [ss:bp-20]
    mov [r_di], ax
    mov word [r_ax], 0
    mov byte [request_status], 0
    mov byte [io_done], 0
    mov ax, cs
    cli
    mov ss, ax
    mov sp, CORE_STACK
    sti
    cld
    call history_begin
    call trace_request
    jmp disk_dispatch
.busy:
    mov word [ss:bp-6], 8000h
    or word [ss:bp+6], 201h
    popa
    pop es
    pop ds
    pop bp
    iret

disk_dispatch:
    mov al, [request_fn]
    cmp al, 1
    je .last
    cmp al, 8
    je .geometry
    cmp al, 15h
    je .type
    cmp byte [fatal_error], 0
    jne disk_hardware_error
    cmp al, 0
    je disk_finish
    cmp al, 2
    jb .invalid
    cmp al, 4
    jbe .transfer
    cmp al, 5
    je .format
    cmp al, 9
    je disk_finish
    cmp al, 0ch
    je .seek
    cmp al, 0dh
    je disk_finish
    cmp al, 10h
    je disk_finish
    cmp al, 11h
    je disk_finish
    cmp al, 14h
    je disk_finish
.invalid:
    mov byte [request_status], 1
    jmp disk_finish
.last:
    mov al, [last_status]
    mov [request_status], al
    jmp disk_finish
.geometry:
    mov word [r_cx], 0ef50h       ; maximum cylinder 495, 16 sectors
    mov ax, 0300h                ; maximum head 3
    mov al, [drive_count]
    mov [r_dx], ax
    ; HDD AH08 preserves BX and ES:DI; floppy DPT return is inapplicable.
    jmp disk_finish
.type:
    mov word [r_ax], 0300h
    mov word [r_cx], 0
    mov word [r_dx], LOGICAL_SECTORS
    jmp disk_finish
.seek:
    mov word [io_count], 1
    call validate_transfer
    jmp disk_finish
.format:
    ; Fixed geometry: logically clear one track; no physical low-level format.
    mov word [io_count], 16
    push word [r_cx]
    and word [r_cx], 0ffc0h
    or word [r_cx], 1
    call validate_transfer
    pop word [r_cx]
    jc disk_finish
.format_loop:
    call ensure_space
    jc disk_hardware_error
    push ds
    pop es
    xor ax, ax
    mov di, sector_buf
    mov cx, 256
    rep stosw
    mov ax, [io_lba]
    mov [record_lba], ax
    call append_record
    jc disk_hardware_error
    inc word [io_lba]
    inc byte [io_done]
    dec word [io_count]
    jnz .format_loop
    mov byte [r_ax], 16
    jmp disk_finish
.transfer:
    call validate_transfer
    jc disk_finish
.loop:
    call trace_sector
    cmp byte [request_fn], 3
    je .write
    mov ax, [io_lba]
    call ftl_read
    jc disk_hardware_error
    cmp byte [request_fn], 4
    je .completed
    call copy_to_caller
    jmp .completed
.write:
    call ensure_space
    jc disk_hardware_error
    call copy_from_caller
    mov ax, [io_lba]
    mov [record_lba], ax
    call append_record
    jc disk_hardware_error
.completed:
    inc byte [io_done]
    inc word [io_lba]
    add word [buffer_off], 512
    dec word [io_count]
    jnz .loop
    mov al, [io_done]
    mov [r_ax], al
    jmp disk_finish

disk_hardware_error:
    mov byte [request_status], 20h
    cmp word [flash_status], 0fffdh
    jne .count
    mov byte [request_status], 10h
.count:
    mov al, [io_done]
    mov [r_ax], al
    jmp disk_finish

; CHS/count validation. Normalize transfer pointer to offset 0..15 so a
; maximum 127-sector transfer cannot wrap the segment's 64 KiB offset.
validate_transfer:
    cmp word [io_count], 1
    jb .bad
    cmp word [io_count], 127
    ja .bad
    mov ax, [r_cx]
    mov bl, ah
    xor bh, bh
    and ax, 0c0h
    shl ax, 2
    or bx, ax
    cmp bx, 496
    jae .bad
    mov ax, [r_dx]
    mov al, ah
    xor ah, ah
    cmp ax, 4
    jae .bad
    shl bx, 2
    add bx, ax
    shl bx, 4
    mov ax, [r_cx]
    and ax, 3fh
    cmp ax, 1
    jb .bad
    cmp ax, 16
    ja .bad
    dec ax
    add ax, bx
    mov [io_lba], ax
    add ax, [io_count]
    cmp ax, LOGICAL_SECTORS
    ja .bad
    cmp byte [request_fn], 4
    je .ok
    cmp byte [request_fn], 5
    je .ok
    cmp byte [request_fn], 0ch
    je .ok
    mov ax, [r_es]
    mov dx, ax
    shl ax, 4
    mov cl, 12
    shr dx, cl
    add ax, [r_bx]
    adc dx, 0
    mov bx, [io_count]
    mov cl, 9
    shl bx, cl
    add ax, bx
    adc dx, 0
    cmp dx, 10h
    ja .boundary
    jb .normalize
    test ax, ax
    jnz .boundary
.normalize:
    mov ax, [r_bx]
    shr ax, 4
    add ax, [r_es]
    mov [buffer_seg], ax
    mov ax, [r_bx]
    and ax, 15
    mov [buffer_off], ax
.ok:
    clc
    ret
.boundary:
    mov byte [request_status], 9
    stc
    ret
.bad:
    mov byte [request_status], 4
    stc
    ret

copy_from_caller:
    pusha
    push ds
    push es
    mov ax, [buffer_seg]
    mov si, [buffer_off]
    push ds
    pop es
    mov ds, ax
    mov di, sector_buf
    mov cx, 256
    rep movsw
    pop es
    pop ds
    popa
    ret
copy_to_caller:
    pusha
    push es
    mov ax, [buffer_seg]
    mov es, ax
    mov di, [buffer_off]
    mov si, sector_buf
    mov cx, 256
    rep movsw
    pop es
    popa
    ret

disk_finish:
    mov word [cs:dbg_phase], 4
    call trace_result
    mov al, [request_status]
    mov [last_status], al
    test al, al
    jz .frame
    mov [r_ax+1], al
.frame:
    call history_end
    mov ax, [entry_ss]
    mov es, ax
    mov di, [entry_bp]
    mov ax, [r_ax]
    mov [es:di-6], ax
    mov ax, [r_bx]
    mov [es:di-12], ax
    mov ax, [r_cx]
    mov [es:di-8], ax
    mov ax, [r_dx]
    mov [es:di-10], ax
    mov ax, [r_es]
    mov [es:di-4], ax
    mov ax, [r_si]
    mov [es:di-18], ax
    mov ax, [r_di]
    mov [es:di-20], ax
    and word [es:di+6], 0fffeh
    or word [es:di+6], 200h
    cmp byte [request_status], 0
    je .bda
    or word [es:di+6], 1
.bda:
    xor ax, ax
    mov es, ax
    mov al, [request_status]
    mov [es:474h], al
    cli
    mov byte [io_busy], 0
    mov ax, [entry_ss]
    mov bx, [entry_bp]
    sub bx, 20
    mov ss, ax
    mov sp, bx
    popa
    pop es
    pop ds
    pop bp
    iret
