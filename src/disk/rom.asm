; AT29C257 32 KiB option ROM; relocate resident core and reserve its map.
CPU 286
BITS 16
ORG 0
%define RAM_KB 80
%define SAVED_SS (16384-6)
%define SAVED_SP (16384-4)
%define SAVED_KB (16384-2)
    db 55h,0aah,32
    jmp short init
    db 'Slaton Computers 16MB ISA Flash',0
init:
    pushf
    pusha
    push ds
    push es
    cld
    xor ax, ax
    mov ds, ax
    mov ax, [413h]
    cmp ax, 256
    jb .return
    mov bp, ax
    sub ax, RAM_KB
    mov [413h], ax
    mov cl, 6
    shl ax, cl
    mov es, ax
    push cs
    pop ds
    mov si, payload
    xor di, di
    mov cx, payload_end-payload
    rep movsb
    ; The payload includes an init-only overlay above 16 KiB. Save the
    ; return frame AFTER copying, so its padding cannot overwrite it.
    mov [es:SAVED_KB], bp
    mov dx, ss
    mov [es:SAVED_SS], dx
    mov [es:SAVED_SP], sp
    mov ax, es
    cli
    mov ss, ax
    mov sp, SAVED_SS
    mov ds, ax
    sti
    mov bx, cs
    push cs
    push word .resume
    push ax
    push word 0
    retf
.resume:
    cli
    mov di, ax
    mov ax, [ss:SAVED_SS]
    mov bx, [ss:SAVED_SP]
    mov cx, [ss:SAVED_KB]
    mov ss, ax
    mov sp, bx
    test di, di
    jnz .return
    xor ax, ax
    mov ds, ax
    mov [413h], cx
.return:
    pop es
    pop ds
    popa
    popf
    retf
align 16, db 0ffh
payload:
    incbin "disk_core.bin"
payload_end:
    times 07f7fh-($-$$) db 0ffh
    db 0 ; full-image balancing byte, outside declared checksum/config pages
    times 32768-($-$$) db 0ffh
