; Disk console: BIOS page-zero text output and COM1.
puts:
    pusha
.character:
    lodsb
    test al, al
    jz .done
    push ax
    push si
    push ds
    mov ah, 0eh
    mov bx, 0007h
    int 10h
    pop ds
    pop si
    pop ax
    call serial_putchar
    jmp .character
.done:
    popa
    ret

; Runtime serial state is always in resident memory, not the setup overlay.
; OFF paths return before any port access, including detection and polling.
serial_debug db 0
serial_enabled db 0
serial_base dw 03f8h
serial_divisor dw 1
serial_lcr db 3
serial_putchar:
    pushf
    pusha
    cmp byte [cs:serial_debug], 0
    je .done
    cmp byte [cs:serial_enabled], 0
    je .done
    mov bl, al
    mov dx, [cs:serial_base]
    add dx, 5
    mov cx, 0ffffh
.wait:
    in al, dx
    test al, 20h
    jnz .send
    loop .wait
    inc word [cs:dbg_uart_timeouts]
    jmp .done
.send:
    mov dx, [cs:serial_base]
    mov al, bl
    out dx, al
.done:
    popa
    popf
    ret

hexword:
    pusha
    mov bx, ax
    mov di, hexbuffer
    mov cx, 4
.digit:
    rol bx, 4
    mov al, bl
    and al, 0fh
    add al, '0'
    cmp al, '9'
    jbe .store
    add al, 7
.store:
    mov [di], al
    inc di
    loop .digit
    mov si, hexbuffer
    call puts
    popa
    ret
space:
    push si
    mov si, space_text
    call puts
    pop si
    ret
newline:
    push si
    mov si, newline_text
    call puts
    pop si
    ret


hexbuffer db '0000',0
space_text db ' ',0
newline_text db 13,10,0

; POST may leave a graphics logo, a nonzero page, or a cursor in a status
; panel. Establish a clean 80x25 text display matching puts' BH=0 output.
; Preserve monochrome mode 7; otherwise select standard color text mode 3.
; Serial output remains a continuous log.
clear_screen:
    pushf
    pusha
    push ds
    push es
    mov ah, 0fh
    int 10h
    and al, 7fh
    cmp al, 7
    je .mono
    mov ax, 3
    jmp .set
.mono:
    mov ax, 7
.set:
    int 10h
    mov ax, 0500h
    int 10h
    mov ah, 02h
    xor bx, bx
    xor dx, dx
    int 10h
    pop es
    pop ds
    popa
    popf
    ret

; Reassert explicitly selected diagnostic settings. Never clear the FIFO.
serial_claim:
    pushf
    pusha
    cmp byte [cs:serial_debug], 0
    je .done
    cli
    mov dx, [cs:serial_base]
    add dx, 5
    in al, dx
    cmp al, 0ffh
    je .done
    mov dx, [cs:serial_base]
    add dx, 3
    mov al, [cs:serial_lcr]
    out dx, al
    sub dx, 2
    xor al, al
    out dx, al
    add dx, 2
    mov al, [cs:serial_lcr]
    or al, 80h
    out dx, al
    sub dx, 3
    mov ax, [cs:serial_divisor]
    out dx, al
    inc dx
    mov al, ah
    out dx, al
    add dx, 2
    mov al, [cs:serial_lcr]
    out dx, al
    inc dx
    mov al, 3
    out dx, al
    mov byte [cs:serial_enabled], 1
.done:
    popa
    popf
    ret
