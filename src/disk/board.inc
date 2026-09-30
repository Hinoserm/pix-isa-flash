; Control bit 20h selects the second flash bank.
; Intel ISA flash board primitives.
; CPU 286, real mode. Raw registers deliberately do NOT assume linear 16 MB.
; Every board I/O instruction uses a 16-bit access.

%ifndef IO_BASE
%define IO_BASE 0300h
%endif

; AX=raw page register, BX=raw word-address register, CX=raw selector.
; Preserves all registers and FLAGS. No flash data is changed.
board_select:
    pushf
    push ax
    push dx
    mov dx, IO_BASE
    out dx, ax
    mov ax, bx
    add dx, 2
    out dx, ax
    mov ax, cx
    add dx, 2
    out dx, ax
    pop dx
    pop ax
    popf
    ret

; Read currently selected word. AX=result, all other registers preserved.
board_read:
    push dx
    mov dx, IO_BASE+8
    mov ax, 8
    or ax, [cs:cur_bank]
    out dx, ax
    sub dx, 2
    in ax, dx
    pop dx
    ret

; Issue AX to the currently selected flash address. Preserves registers.
; Used for Intel read-array, identification, erase and buffered-program commands.
board_command:
    pushf
    push ax
    push dx
    mov dx, IO_BASE+6
    out dx, ax
    add dx, 2
    mov ax, 14h
    or ax, [cs:cur_bank]
    out dx, ax
    mov ax, 10h
    or ax, [cs:cur_bank]
    out dx, ax
    pop dx
    pop ax
    popf
    ret

; Initialize the board control state, then wait for readiness.
board_reset:
    push ax
    push dx
    mov dx, IO_BASE+8
    xor ax, ax
    out dx, ax
    mov ax, 14h
    or ax, [cs:cur_bank]
    out dx, ax
    pop dx
    pop ax
    jmp board_ready

; Poll the board's ready indication. CF=1 on timeout. Registers preserved.
board_ready:
    push ax
    push cx
    push dx
    mov dx, IO_BASE+10
    mov cx, 0ffffh
.poll:
    in ax, dx
    test ax, 20h
    jnz .ok
    loop .poll
    pop dx
    pop cx
    pop ax
    stc
    ret
.ok:
    pop dx
    pop cx
    pop ax
    clc
    ret
