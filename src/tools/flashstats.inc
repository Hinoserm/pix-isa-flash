; Read-only Intel/Slaton metadata statistics, shared by POST setup and DOS.
; DS=CS; scratch 8000h..9D7Fh, outside code/stack. No data programming/erase.
; No UART accesses. CF=1 if the two Intel chips cannot be identified.
st_state equ 8000h
st_genlo equ 8080h
st_genhi equ 8180h
st_srclo equ 8280h
st_srchi equ 8380h
st_source equ 8480h
st_header equ 8580h
st_meta equ 85a0h
st_seen equ 8d80h
st_counts equ 9d00h
st_active equ st_counts+0
st_blank equ st_counts+2
st_invalid equ st_counts+4
st_retired equ st_counts+6
st_pending equ st_counts+8
st_live equ st_counts+10
st_committed equ st_counts+12
st_obsolete equ st_counts+14
st_partial equ st_counts+16
st_badrecords equ st_counts+18
st_tailfree equ st_counts+20
st_erase equ st_counts+22
st_available equ st_counts+24
st_unreserved equ st_counts+26
st_block equ 9d30h
st_slot equ 9d32h
st_tail equ 9d34h
st_work_end equ 9d80h

stats_scan:
    pushf
    pusha
    push es
    push ds
    pop es
    cld
    xor ax, ax
    mov di, st_state
    mov cx, st_work_end-st_state
    rep stosb
    call st_identify
    jc .fail
    mov word [st_block], 64
    call st_identify
    jc .fail
    mov word [st_block], 0
.headers:
    mov ax, [st_block]
    xor bx, bx
    mov cx, 16
    mov di, st_header
    call st_read
    call st_valid_header
    jc .unallocated
    mov bx, [st_block]
    mov byte [st_state+bx], 2
    cmp word [st_header+28], 0ffffh
    jne .save
    mov byte [st_state+bx], 3
.save:
    shl bx, 1
    mov ax, [st_header+8]
    mov [st_genlo+bx], ax
    mov ax, [st_header+10]
    mov [st_genhi+bx], ax
    mov ax, [st_header+16]
    mov [st_srclo+bx], ax
    mov ax, [st_header+18]
    mov [st_srchi+bx], ax
    mov ax, [st_header+20]
    mov [st_source+bx], ax
    jmp .next_header
.unallocated:
    mov di, st_header
    mov ax, 0ffffh
    mov cx, 16
    repe scasw
    mov bx, [st_block]
    je .next_header
    mov byte [st_state+bx], 1
.next_header:
    inc word [st_block]
    cmp word [st_block], 128
    jb .headers
    ; Follow exact source generations of committed GC replacements.
    xor bx, bx
.retire:
    cmp byte [st_state+bx], 2
    je .source
    cmp byte [st_state+bx], 4
    jne .next_retire
.source:
    mov si, bx
    shl si, 1
    mov di, [st_source+si]
    cmp di, 128
    jae .next_retire
    cmp byte [st_state+di], 2
    je .generation
    cmp byte [st_state+di], 4
    jne .next_retire
.generation:
    shl di, 1
    mov ax, [st_srclo+si]
    cmp ax, [st_genlo+di]
    jne .next_retire
    mov ax, [st_srchi+si]
    cmp ax, [st_genhi+di]
    jne .next_retire
    shr di, 1
    mov byte [st_state+di], 4
.next_retire:
    inc bx
    cmp bx, 128
    jb .retire
    mov word [st_block], 0
.records:
    mov bx, [st_block]
    mov al, [st_state+bx]
    cmp al, 0
    je .blank
    cmp al, 1
    je .invalid
    cmp al, 3
    je .pending
    cmp al, 4
    je .retired
    inc word [st_active]
    mov ax, bx
    mov bx, 16
    mov di, st_meta
    mov cx, 1008
    call st_read
    mov word [st_slot], 0
    mov word [st_tail], 0
.slot:
    mov si, [st_slot]
    shl si, 3
    add si, st_meta
    mov ax, [si]
    and ax, [si+2]
    and ax, [si+4]
    and ax, [si+6]
    cmp ax, 0ffffh
    je .next_slot
    mov ax, [st_slot]
    inc ax
    mov [st_tail], ax
    cmp word [si+6], 0
    jne .partial
    mov ax, [si]
    cmp ax, 31744
    jae .badrecord
    mov dx, ax
    xor dx, [si+2]
    cmp dx, 0ffffh
    jne .badrecord
    inc word [st_committed]
    mov bx, ax
    shr bx, 3
    mov cl, al
    and cl, 7
    mov al, 1
    shl al, cl
    test [st_seen+bx], al
    jnz .next_slot
    or [st_seen+bx], al
    inc word [st_live]
    jmp .next_slot
.partial:
    inc word [st_partial]
    jmp .next_slot
.badrecord:
    inc word [st_badrecords]
.next_slot:
    inc word [st_slot]
    cmp word [st_slot], 252
    jb .slot
    mov ax, 252
    sub ax, [st_tail]
    add [st_tailfree], ax
    jmp .next_block
.blank:
    inc word [st_blank]
    jmp .next_block
.invalid:
    inc word [st_invalid]
    jmp .next_block
.pending:
    inc word [st_pending]
    jmp .next_block
.retired:
    inc word [st_retired]
.next_block:
    inc word [st_block]
    cmp word [st_block], 128
    jb .records
    mov ax, [st_committed]
    sub ax, [st_live]
    mov [st_obsolete], ax
    mov ax, [st_invalid]
    add ax, [st_retired]
    mov [st_erase], ax
    add ax, [st_blank]
    mov [st_available], ax
    sub ax, 2
    jae .reserve
    xor ax, ax
.reserve:
    mov [st_unreserved], ax
    pop es
    popa
    popf
    clc
    ret
.fail:
    pop es
    popa
    popf
    stc
    ret

; AX=physical block, BX=word offset, CX=count, DS:DI=destination.
; Mask IRQs across the address/data burst so a TSR cannot interleave latches.
st_read:
    pushf
    cli
    pusha
    push es
    push ds
    pop es
    cld
    mov si, ax
    and si, 40h
    shr si, 1
    and ax, 3fh
    mov dx, 300h
    out dx, ax
    mov dx, 304h
    xor ax, ax
    out dx, ax
.loop:
    mov dx, 302h
    mov ax, bx
    out dx, ax
    mov dx, 308h
    mov ax, si
    or ax, 8
    out dx, ax
    mov dx, 306h
    in ax, dx
    stosw
    inc bx
    loop .loop
    pop es
    popa
    popf
    ret
st_command:
    pusha
    mov dx, 306h
    out dx, ax
    mov ax, [st_block]
    and ax, 40h
    shr ax, 1
    mov bx, ax
    mov dx, 308h
    or ax, 14h
    out dx, ax
    mov ax, bx
    or ax, 10h
    out dx, ax
    popa
    ret
st_identify:
    pushf
    cli
    pusha
    mov ax, [st_block]
    xor bx, bx
    mov cx, 1
    mov di, st_header
    call st_read
    mov ax, 90h
    call st_command
    call st_ready
    jc .bad
    mov ax, [st_block]
    xor bx, bx
    mov cx, 2
    mov di, st_header
    call st_read
    mov ax, 0ffh
    call st_command
    cmp word [st_header], 89h
    jne .bad
    cmp word [st_header+2], 15h
    jne .bad
    popa
    popf
    clc
    ret
.bad:
    mov ax, 0ffh
    call st_command
    popa
    popf
    stc
    ret

st_ready:
    push ax
    push cx
    push dx
    mov dx, 30ah
    mov cx, 0ffffh
.wait:
    in ax, dx
    test ax, 20h
    jnz .ok
    loop .wait
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

st_valid_header:
    cmp word [st_header], 'PX'
    jne .bad
    cmp word [st_header+2], 'DF'
    jne .bad
    cmp word [st_header+4], '00'
    jne .bad
    cmp word [st_header+6], '01'
    jne .bad
    cmp word [st_header+30], 0
    jne .bad
    cmp word [st_header+26], 31744
    jne .bad
    mov ax, [st_header+8]
    mov dx, [st_header+10]
    mov bx, ax
    or bx, dx
    jz .bad
    xor ax, [st_header+12]
    xor dx, [st_header+14]
    and ax, dx
    cmp ax, 0ffffh
    jne .bad
    mov si, st_header
    mov cx, 22
    call st_crc
    cmp ax, [st_header+22]
    jne .bad
    not ax
    cmp ax, [st_header+24]
    jne .bad
    cmp word [st_header+28], 0fffeh
    jb .bad
    mov bx, [st_header+20]
    cmp bx, 0ffffh
    je .normal
    cmp bx, 128
    jae .bad
    mov ax, [st_header+18]
    cmp ax, [st_header+10]
    jb .ok
    ja .bad
    mov ax, [st_header+16]
    cmp ax, [st_header+8]
    jae .bad
.ok:
    clc
    ret
.normal:
    mov ax, [st_header+16]
    or ax, [st_header+18]
    jnz .bad
    cmp word [st_header+28], 0fffeh
    jne .bad
    clc
    ret
.bad:
    stc
    ret
st_crc:
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

st_labels dw st_l_active,st_l_blank,st_l_invalid,st_l_retired,st_l_pending
          dw st_l_live,st_l_committed,st_l_obsolete,st_l_partial,st_l_bad,st_l_tail,st_l_erase,st_l_available,st_l_unreserved
st_l_active db 'Allocated blocks (current generations)',0
st_l_blank db 'Unallocated blocks (blank headers)',0
st_l_invalid db 'Invalid / quick-erased block headers',0
st_l_retired db 'Retired blocks (reclaimable)',0
st_l_pending db 'Pending copy/recovery blocks',0
st_l_live db 'Written logical sectors / 31744',0
st_l_committed db 'Committed sector records',0
st_l_obsolete db 'Obsolete committed records',0
st_l_partial db 'Incomplete record writes',0
st_l_bad db 'Invalid committed record metadata',0
st_l_tail db 'Trailing slots in allocated blocks',0
st_l_erase db 'Invalid + retired blocks needing erase',0

st_l_available db 'Unallocated / reclaimable blocks',0
st_l_unreserved db 'Free blocks above 2-block reserve',0
