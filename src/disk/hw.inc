; Intel J5 hardware interface. DS=resident segment. All accesses are words.
%include "disk/board.inc"
select:
    pusha
    mov ax, [cur_block]
    mov dx, ax
    and dx, 40h
    shr dx, 1
    mov [cur_bank], dx
    and ax, 3fh
    mov bx, [cur_word]
    xor cx, cx
    call board_select
    popa
    ret
command:
    call select
    call board_command
    ret
; DS:DI destination, CX word count. Preserve registers; advance cur_word.
read_words:
    push ax
    mov al, 'R'
    call trace_hw
    pop ax
    pusha
    push es
    push ds
    pop es
    call select
    mov bx, [cur_word]
.loop:
    mov dx, 302h
    mov ax, bx
    out dx, ax
    mov dx, 308h
    mov ax, [cur_bank]
    or ax, 8
    out dx, ax
    mov dx, 306h
    in ax, dx
    stosw
    inc bx
    loop .loop
    mov [cur_word], bx
    pop es
    popa
    ret
identify:
    pusha
    push word [cur_word]
    mov word [cur_word], 0
    mov ax, 90h
    call command
    call board_ready
    jc .bad
    mov di, verify_buf
    mov cx, 2
    call read_words
    cmp word [verify_buf], 89h
    jne .bad
    cmp word [verify_buf+2], 15h
    jne .bad
    mov ax, 0ffh
    call command
    pop word [cur_word]
    popa
    clc
    ret
.bad:
    mov ax, 0ffh
    call command
    pop word [cur_word]
    popa
    stc
    ret
; Wait for board readiness and Intel SR.7, with approximately 30 seconds of
; BIOS tick transitions. Interrupts run while polling. A large iteration
; watchdog also terminates if the BIOS timer has stopped. Tick-transition
; counting tolerates both the 16-bit wrap and midnight reset.
wait_flash:
    pushf
    pusha
    push es
    mov ax, 40h
    mov es, ax
    mov bp, [es:6ch]
    mov di, 550
    mov word [watch_low], 0
    mov word [watch_high], 1024
    sti
.poll:
    mov dx, 30ah
    in ax, dx
    test ax, 20h
    jz .time
    mov dx, 308h
    mov ax, [cur_bank]
    or ax, 8
    out dx, ax
    mov ax, [cur_bank]
    out dx, ax
    or ax, 8
    out dx, ax
    mov dx, 306h
    in ax, dx
    test ax, 80h
    jnz .ready
    ; Intel requires another E8h setup when XSR says buffer unavailable.
    ; Only the buffer-availability wait uses error_mask=0.
    cmp word [error_mask], 0
    jne .time
    mov ax, 0e8h
    call command
    jmp .time
.ready:
    mov [flash_status], ax
    cmp ax, 0ffffh
    je .bad
    test ax, [error_mask]
    jnz .bad
    pop es
    popa
    popf
    clc
    ret
.time:
    mov ax, [es:6ch]
    cmp ax, bp
    je .watch
    mov bp, ax
    dec di
    jz .timeout
.watch:
    dec word [watch_low]
    jnz .poll
    dec word [watch_high]
    jnz .poll
.timeout:
    mov word [flash_status], 0fffeh
.bad:
    pop es
    popa
    popf
    stc
    ret

erase_block:
    push ax
    mov al, 'E'
    call trace_hw
    pop ax
    pusha
    push word [cur_word]
    mov word [cur_word], 0
    mov ax, 50h
    call command
    mov ax, 20h
    call command
    mov ax, 0d0h
    call command
    mov word [error_mask], 3ah
    call wait_flash
    pushf
    mov ax, 0ffh
    call command
    popf
    pop word [cur_word]
    popa
    call trace_hw_done
    ret


; Program 1..16 words DS:SI, within a single 32-byte write buffer.
; Preserve registers and cur_word. Verify exact programmed words.
program_words:
    push ax
    mov al, 'P'
    call trace_hw
    pop ax
    pusha
    push es
    push word [cur_word]
    mov [prog_src], si
    mov [prog_count], cx
    mov ax, [cur_word]
    mov [prog_start], ax
    and ax, 15
    add ax, cx
    cmp ax, 16
    ja .invalid
    jcxz .invalid
    mov ax, 50h
    call command
    mov ax, 0e8h
    call command
    mov word [error_mask], 0
    call wait_flash
    jc .bad
    mov ax, cx
    dec ax
    call command
.data:
    lodsw
    call command
    inc word [cur_word]
    loop .data
    mov ax, [prog_start]
    mov [cur_word], ax
    mov ax, 0d0h
    call command
    mov word [error_mask], 3ah
    call wait_flash
    jc .bad
    mov ax, 0ffh
    call command
    mov ax, [prog_start]
    mov [cur_word], ax
    mov cx, [prog_count]
    mov di, verify_buf
    call read_words
    push ds
    pop es
    mov si, [prog_src]
    mov di, verify_buf
    repe cmpsw
    jne .mismatch
    pop word [cur_word]
    pop es
    popa
    clc
    call trace_hw_done
    ret
.invalid:
    mov word [flash_status], 0fffch
    jmp .bad
.mismatch:
    mov word [flash_status], 0fffdh
.bad:
    mov ax, 0ffh
    call command
    pop word [cur_word]
    pop es
    popa
    stc
    call trace_hw_done
    ret
; CRC16-CCITT, init FFFF. DS:SI bytes, CX count; AX result.
crc16:
    push bx
    push cx
    push dx
    push si
    mov ax, 0ffffh
.byte:
    mov dl, [si]
    inc si
    xor ah, dl
    mov bl, 8
.bit:
    shl ax, 1
    jnc .next
    xor ax, 1021h
.next:
    dec bl
    jnz .bit
    loop .byte
    pop si
    pop dx
    pop cx
    pop bx
    ret
