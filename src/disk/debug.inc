; Serial-only diagnostics: never call video or disk BIOS. Preserve FLAGS,
; all general registers and segments. This avoids modifying disk requests.
trace_puts:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    push ds
    push cs
    pop ds
    cld
    call serial_claim
.next:
    lodsb
    test al, al
    jz .done
    call serial_putchar
    jmp .next
.done:
    pop ds
    popa
    popf
    ret
trace_hex:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    mov bx, ax
    mov cx, 4
.next:
    rol bx, 4
    mov al, bl
    and al, 15
    add al, '0'
    cmp al, '9'
    jbe .emit
    add al, 7
.emit:
    call serial_putchar
    loop .next
    popa
    popf
    ret
trace_nl:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    push si
    mov si, dbg_newline
    call trace_puts
    pop si
    ret
trace_space:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    push ax
    mov al, ' '
    call serial_putchar
    pop ax
    popf
    ret
trace_platform:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    push es
    xor ax, ax
    mov es, ax
    mov si, dbg_platform
    call trace_puts
    mov ax, cs
    call trace_hex
    call trace_space
    mov ax, [es:472h]
    call trace_hex
    call trace_space
    mov ax, [es:4eh]
    call trace_hex
    call trace_space
    mov ax, [es:4ch]
    call trace_hex
    call trace_space
    mov ax, [es:66h]
    call trace_hex
    call trace_space
    mov ax, [es:64h]
    call trace_hex
    call trace_nl
    pop es
    popa
    popf
    ret

trace_request:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    mov si, dbg_request
    call trace_puts
    mov ax, [request_ax]
    call trace_hex
    call trace_space
    mov ax, [r_cx]
    call trace_hex
    call trace_space
    mov ax, [r_dx]
    call trace_hex
    call trace_space
    mov ax, [r_es]
    call trace_hex
    call trace_space
    mov ax, [r_bx]
    call trace_hex
    call trace_nl
    popa
    popf
    ret
trace_result:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    mov si, dbg_result
    call trace_puts
    xor ax, ax
    mov al, [request_status]
    call trace_hex
    call trace_space
    mov al, [io_done]
    call trace_hex
    call trace_space
    mov ax, [flash_status]
    call trace_hex
    call trace_space
    mov ax, [cur_block]
    call trace_hex
    call trace_space
    mov ax, [cur_word]
    call trace_hex
    call trace_nl
    popa
    popf
    ret
dbg_newline db 13,10,0
dbg_init db '[SC14] RAM core entered',13,10,0
dbg_platform db '[SC14] CS warm I13seg off I19seg off: ',0
dbg_mount db '[SC14] mount begin',13,10,0
dbg_mounted db '[SC14] mount OK; free blocks: ',0
dbg_cfg db '[SC14] saved disk mode (0=80h,1=81h,2=off): ',0
dbg_installed db '[SC14] disk installed; INT19/boot order unchanged',13,10,0
dbg_failure db '[SC14] initialization failed; flash status: ',0
dbg_request db '[SC14] INT13 AX CX DX ES BX: ',0
dbg_result db '[SC14] DONE status count flash block word: ',0

trace_ftl_failure:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    mov si, dbg_ftl
    call trace_puts
    xor ax, ax
    mov al, [debug_stage]
    call trace_hex
    call trace_space
    mov ax, [mount_block]
    call trace_hex
    call trace_space
    mov ax, [mount_slot]
    call trace_hex
    call trace_space
    mov ax, [io_lba]
    call trace_hex
    call trace_space
    mov ax, [flash_status]
    call trace_hex
    call trace_nl
    popa
    popf
    ret
debug_stage db 0
dbg_ftl db '[SC14] FTL FAIL stage mount-block slot LBA flash: ',0


trace_native:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    push ds
    push cs
    pop ds
    mov si, dbg_native
    call trace_puts
    call trace_hex
    call trace_space
    mov ax, cx
    call trace_hex
    call trace_space
    mov ax, dx
    call trace_hex
    call trace_nl
.done:
    pop ds
    popa
    popf
    ret
dbg_native db '[SC14] native -> previous BIOS AX CX DX: ',0

trace_sector:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    mov si, dbg_sector
    call trace_puts
    mov ax, [io_lba]
    call trace_hex
    call trace_space
    mov ax, [io_count]
    call trace_hex
    call trace_space
    mov ax, [buffer_seg]
    call trace_hex
    call trace_space
    mov ax, [buffer_off]
    call trace_hex
    call trace_nl
    popa
    popf
    ret
; AL=R/P/E. Called before array reads, buffered programs, and block erases.
; Prints every operation, not every individual IN/OUT bus cycle.
trace_hw:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    mov si, dbg_hw
    call trace_puts
    call serial_putchar
    call trace_space
    mov ax, [cur_block]
    call trace_hex
    call trace_space
    mov ax, [cur_word]
    call trace_hex
    call trace_space
    mov ax, cx
    call trace_hex
    call trace_nl
    popa
    popf
    ret
dbg_sector db '[SC14] SECTOR LBA remaining segment offset: ',0
dbg_hw db '[SC14] HW op block word CX: ',0

trace_absent:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    push ds
    push cs
    pop ds
    mov si, dbg_absent
    call trace_puts
    call trace_hex
    call trace_space
    mov ax, cx
    call trace_hex
    call trace_space
    mov ax, dx
    call trace_hex
    call trace_nl
.done:
    pop ds
    popa
    popf
    ret
dbg_absent db '[SC14] nonexistent disk rejected AX CX DX: ',0

trace_hw_done:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    mov bp, sp
    mov si, dbg_hw_done
    call trace_puts
    mov ax, [ss:bp+16]
    and ax, 1
    call trace_hex
    call trace_space
    mov ax, [flash_status]
    call trace_hex
    call trace_nl
    popa
    popf
    ret
dbg_hw_done db '[SC14] HW DONE carry status: ',0

; 32 x 64-byte records in the unused final 2 KiB of the existing map segment.
; Only owned-disk calls; all helpers preserve caller registers and FLAGS.
history_begin:
    pushf
    pusha
    push es
    mov ax, [map_segment]
    mov es, ax
    mov di, [history_head]
    shl di, 6
    add di, 0f800h
    mov [history_current], di
    inc word [history_head]
    and word [history_head], 31
    mov ax, [dbg_calls]
    stosw
    mov ax, [dbg_calls+2]
    stosw
    mov ax, [request_ax]
    stosw
    mov ax, [r_cx]
    stosw
    mov ax, [r_dx]
    stosw
    mov ax, [r_es]
    stosw
    mov ax, [r_bx]
    stosw
    mov ax, [entry_ss]
    stosw
    mov ax, [entry_bp]
    stosw
    push es
    mov bx, ax
    mov ax, [entry_ss]
    mov es, ax
    mov ax, [es:bx+2]
    mov dx, [es:bx+4]
    mov cx, [es:bx+6]
    pop es
    stosw
    mov ax, dx
    stosw
    mov ax, cx
    stosw
    mov ax, 0ffffh
    stosw
    stosw
    stosw
    xor ax, ax
    stosw
    mov cx, 16
    rep stosw
    pop es
    popa
    popf
    call trace_history
    ret
history_end:
    pushf
    pusha
    push es
    mov ax, [map_segment]
    mov es, ax
    mov di, [history_current]
    mov ax, [r_ax]
    mov [es:di+24], ax
    mov ax, [es:di+22]
    and ax, 0fffeh
    or ax, 200h
    cmp byte [request_status], 0
    je .flags
    or ax, 1
.flags:
    mov [es:di+26], ax
    mov ax, [io_lba]
    mov [es:di+28], ax
    mov word [es:di+30], 1
    mov ax, [r_cx]
    mov [es:di+32], ax
    mov ax, [r_dx]
    mov [es:di+34], ax
    mov ax, [r_es]
    mov [es:di+36], ax
    mov ax, [r_bx]
    mov [es:di+38], ax
    mov ax, [r_si]
    mov [es:di+40], ax
    mov ax, [r_di]
    mov [es:di+42], ax
    mov ax, [flash_status]
    mov [es:di+44], ax
    mov ax, [cur_block]
    mov [es:di+46], ax
    mov ax, [cur_word]
    mov [es:di+48], ax
    pop es
    popa
    popf
    call trace_history
    ret

trace_history:
    pushf
    cmp byte [cs:serial_debug], 0
    jne .enabled
    popf
    ret
.enabled:
    popf
    pushf
    pusha
    push es
    mov si, dbg_history
    call trace_puts
    mov ax, [map_segment]
    mov es, ax
    mov di, [history_current]
    mov cx, 32
.word:
    mov ax, [es:di]
    call trace_hex
    call trace_space
    add di, 2
    loop .word
    call trace_nl
    pop es
    popa
    popf
    ret
dbg_history db '[SC14] FRAME: ',0
