; Read-only DOS status viewer. No UART writes, BIOS disk calls or emulation.
CPU 286
BITS 16
ORG 100h
start:
    cld
    push cs
    pop ds
    smsw ax
    test ax, 1
    jnz stats_dos_environment_error
    mov ax, [2]
    mov bx, cs
    sub ax, bx
    cmp ax, 1000h
    jb stats_dos_environment_error
    cmp byte [80h], 0
    je .quiet
    mov byte [verbose], 1
.quiet:
    mov dx, stats_dos_heading
    call print
    call stats_scan
    jc stats_dos_no_card
    call stats_dos_report
    cmp byte [verbose], 0
    jne .diagnostics
    mov ax, 4c00h
    int 21h
.diagnostics:
    mov dx, intro
    call print
    mov ax, 3513h
    int 21h
    mov ax, es
    call hexword
    mov dl, ':'
    call character
    mov ax, bx
    call hexword
    mov dx, crlf
    call print
    ; Resident base is KiB aligned by the existing ROM allocator. Search
    ; conventional memory read-only rather than assuming IVT still points in.
    mov bx, 1000h
.scan:
    mov es, bx
    mov di, 3
    mov si, signature
    mov cx, 6
    repe cmpsb
    jne .next
    cmp word [es:9], '12'
    je .matched
    cmp word [es:9], '14'
    je .matched
    cmp word [es:9], '13'
    je .matched
    cmp word [es:9], '11'
    je .matched
    cmp word [es:9], '10'
    je .matched
    cmp byte [es:9], '0'
    jne .next
    cmp byte [es:10], '7'
    jb .next
    cmp byte [es:10], '9'
    ja .next
.matched:
    mov byte [history_valid], 0
    cmp word [es:9], '12'
    je .capture
    cmp word [es:9], '14'
    je .capture
    cmp word [es:9], '13'
    je .capture
    cmp word [es:9], '11'
    je .capture
    cmp word [es:9], '10'
    je .capture
    cmp byte [es:10], '9'
    jne .snapshot
.capture:
    mov ax, [es:27]
    cmp ax, 1000h
    jb .snapshot
    cmp ax, 9000h
    ja .snapshot
    pushf
    cli
    pusha
    push ds
    push es
    mov dx, [es:29]
    mov [ring_head], dx
    mov ds, ax
    push cs
    pop es
    mov si, 0f800h
    mov di, ring_copy
    mov cx, 1024
    rep movsw
    pop es
    pop ds
    popa
    popf
    mov byte [history_valid], 1
.snapshot:
    ; Snapshot fields with IRQs masked; output afterward, no serial reliance.
    pushf
    cli
    mov si, 11
    mov di, snapshot
    mov cx, 8
.copy:
    mov ax, [es:si]
    mov [di], ax
    add si, 2
    add di, 2
    loop .copy
    popf
    mov dx, found
    call print
    mov ax, bx
    call hexword
    mov dx, fields
    call print
    mov ax, [snapshot+2]
    call hexword
    mov ax, [snapshot]
    call hexword
    mov si, snapshot+4
    mov cx, 6
.show:
    mov dl, ' '
    call character
    lodsw
    call hexword
    loop .show
    mov dx, crlf
    call print
    cmp byte [verbose], 0
    je .next
    cmp byte [history_valid], 0
    je .next
    mov dx, ring_msg
    call print
    mov ax, [ring_head]
    call hexword
    mov dx, crlf
    call print
    mov si, ring_copy
    mov bp, 32
.ring_row:
    mov cx, 32
.ring_word:
    lodsw
    call hexword
    mov dl, ' '
    call character
    loop .ring_word
    mov dx, crlf
    call print
    dec bp
    jnz .ring_row
.next:
    add bx, 40h
    cmp bx, 0a000h
    jb .scan
    mov dx, legend
    call print
    mov ax, 4c00h
    int 21h
print:
    pusha
    push es
    mov ah, 09h
    int 21h
    pop es
    popa
    ret
character:
    pusha
    mov ah, 02h
    int 21h
    popa
    ret
hexword:
    pusha
    mov bp, ax
    mov cx, 4
.loop:
    rol bp, 4
    mov dx, bp
    and dl, 15
    add dl, '0'
    cmp dl, '9'
    jbe .out
    add dl, 7
.out:
    call character
    loop .loop
    popa
    ret
signature db 'SCDBG007'
snapshot times 8 dw 0
intro db 'Slaton 0.7-0.14 resident diagnostics. Current INT13 vector: $'
found db 'Resident segment: $'
fields db 13,10,'Calls      AX   CX   DX phase UART-timeouts UART-skips',13,10,'$'
crlf db 13,10,'$'
legend db 'Hex values. Phase: 0=entry 1=flash 2=floppy chain 3=native chain 4=flash done.',13,10
       db 'No resident line means no 0.7-0.14 diagnostic signature was found.',13,10
       db 'Timeout/skip counters count dropped characters and wrap at 65536.',13,10
       db '0.8+ counts hard disks only; floppy requests bypass all diagnostics.',13,10,'$'


history_valid db 0
ring_head dw 0
ring_msg db 'Request history: 32 physical slots, 32 words per row; next slot = $'
ring_copy times 2048 db 0

stats_dos_environment_error:
    mov dx, stats_dos_environment_msg
    call print
    mov ax, 4c01h
    int 21h
stats_dos_no_card:
    mov dx, stats_dos_no_card_msg
    call print
    mov ax, 4c01h
    int 21h
stats_dos_report:
    xor bp, bp
.row:
    mov si, bp
    shl si, 1
    mov si, [st_labels+si]
    call stats_printz
    mov dl, ':'
    call character
    mov dl, ' '
    call character
    mov si, bp
    shl si, 1
    mov ax, [st_counts+si]
    call stats_print_decimal
    mov dx, crlf
    call print
    inc bp
    cmp bp, 14
    jb .row
    mov dx, stats_dos_notes
    call print
    ret
stats_printz:
    pusha
.next:
    lodsb
    test al, al
    jz .done
    mov dl, al
    call character
    jmp .next
.done:
    popa
    ret
stats_print_decimal:
    pusha
    xor cx, cx
    mov bx, 10
.digit:
    xor dx, dx
    div bx
    push dx
    inc cx
    test ax, ax
    jnz .digit
.emit:
    pop dx
    add dl, '0'
    call character
    loop .emit
    popa
    ret
verbose db 0
stats_dos_heading db 'Slaton flash allocation: read-only metadata scan',13,10
                     db '128 blocks x 128 KiB; logical disk 31744 x 512-byte sectors',13,10,'$'
stats_dos_environment_msg db 'Use plain real-mode DOS with at least 64 KiB free memory.',13,10,'$'
stats_dos_no_card_msg db 'Both Intel flash chips could not be identified at I/O 0300h.',13,10,'$'
stats_dos_notes db 'Counts describe flash allocation, NOT DOS filesystem free space.',13,10
               db 'Blank headers do not prove erased payloads. Blocks are erased before reuse.',13,10
               db 'Payload CRCs not checked; pending copies require mount recovery.',13,10
               db 'Use SLSTAT /D for the full RAM request history.',13,10,13,10,'$'
%include "tools/flashstats.inc"
%if ($-$$+100h) > 8000h
    %error "SLSTAT code/data overlap scratch workspace"
%endif
