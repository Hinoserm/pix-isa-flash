; Persistent log mapping. DS=resident segment. CF reports failure.
; All functions preserve general registers unless otherwise stated.
ftl_fail:
    call trace_ftl_failure
    mov byte [fatal_error], 1
    stc
    ret

; Header validity. A torn/unrecognized header contains no committed records.
header_valid:
    cmp word [header_buf+30], 0
    jne .bad
    cmp word [header_buf], 'PX'
    jne .bad
    cmp word [header_buf+2], 'DF'
    jne .bad
    cmp word [header_buf+4], '00'
    jne .bad
    cmp word [header_buf+6], '01'
    jne .bad
    mov ax, [header_buf+8]
    mov dx, [header_buf+10]
    mov bx, ax
    or bx, dx
    jz .bad
    xor ax, [header_buf+12]
    xor dx, [header_buf+14]
    and ax, dx
    cmp ax, 0ffffh
    jne .bad
    cmp word [header_buf+26], LOGICAL_SECTORS
    jne .bad
    mov si, header_buf
    mov cx, 22
    call crc16
    cmp ax, [header_buf+22]
    jne .bad
    not ax
    cmp ax, [header_buf+24]
    jne .bad
    mov ax, [header_buf+28]
    cmp ax, 0fffeh
    jb .bad
    mov bx, [header_buf+20]
    cmp bx, 0ffffh
    je .normal
    cmp bx, 128
    jae .bad
    mov ax, [header_buf+18]
    cmp ax, [header_buf+10]
    jb .ok
    ja .bad
    mov ax, [header_buf+16]
    cmp ax, [header_buf+8]
    jae .bad
.ok:
    clc
    ret
.normal:
    mov ax, [header_buf+16]
    or ax, [header_buf+18]
    jnz .bad
    cmp word [header_buf+28], 0fffeh
    jne .bad
    clc
    ret
.bad:
    stc
    ret

ftl_mount:
    mov byte [debug_stage], 1
    pusha
    push es
    mov ax, [map_segment]
    mov es, ax
    xor di, di
    mov ax, 0ffffh
    mov cx, LOGICAL_SECTORS
    rep stosw
    mov word [free_blocks], 128
    mov word [active_block], 0ffffh
    mov word [mount_block], 0
.headers:
    mov ax, [mount_block]
    mov [cur_block], ax
    mov word [cur_word], 0
    mov di, header_buf
    mov cx, 16
    call read_words
    call header_valid
    jc .next_header
    mov bx, [mount_block]
    mov byte [block_state+bx], 1
    dec word [free_blocks]
    shl bx, 1
    mov ax, [header_buf+8]
    mov [block_gen_lo+bx], ax
    mov dx, [header_buf+10]
    mov [block_gen_hi+bx], dx
    cmp dx, [max_gen_hi]
    ja .new_max
    jb .save_header
    cmp ax, [max_gen_lo]
    jbe .save_header
.new_max:
    mov [max_gen_lo], ax
    mov [max_gen_hi], dx
.save_header:
    mov ax, [header_buf+16]
    mov [block_src_lo+bx], ax
    mov ax, [header_buf+18]
    mov [block_src_hi+bx], ax
    mov ax, [header_buf+20]
    mov [block_source+bx], ax
    mov ax, [header_buf+28]
    mov [block_gc_state+bx], ax
.next_header:
    inc word [mount_block]
    cmp word [mount_block], 128
    jb .headers
    ; Roll back incomplete copy transactions. The source must remain intact.
    mov word [mount_block], 0
.pending:
    mov byte [debug_stage], 2
    mov bx, [mount_block]
    cmp byte [block_state+bx], 1
    jne .next_pending
    shl bx, 1
    cmp word [block_gc_state+bx], 0ffffh
    jne .next_pending
    mov di, [block_source+bx]
    cmp di, 128
    jae .bad
    cmp byte [block_state+di], 1
    jne .bad
    shl di, 1
    cmp word [block_gc_state+di], 0fffeh
    jne .bad
    mov ax, [block_src_lo+bx]
    cmp ax, [block_gen_lo+di]
    jne .bad
    mov ax, [block_src_hi+bx]
    cmp ax, [block_gen_hi+di]
    jne .bad
    mov ax, [mount_block]
    mov [cur_block], ax
    call erase_block
    jc .bad
    mov bx, [mount_block]
    mov byte [block_state+bx], 0
    inc word [free_blocks]
.next_pending:
    inc word [mount_block]
    cmp word [mount_block], 128
    jb .pending
    ; A committed replacement retires only the exact source generation.
    mov word [mount_block], 0
.retire:
    mov byte [debug_stage], 3
    mov bx, [mount_block]
    cmp byte [block_state+bx], 0
    je .next_retire
    shl bx, 1
    mov di, [block_source+bx]
    cmp di, 128
    jae .next_retire
    cmp byte [block_state+di], 0
    je .next_retire
    shl di, 1
    mov ax, [block_src_lo+bx]
    cmp ax, [block_gen_lo+di]
    jne .next_retire
    mov ax, [block_src_hi+bx]
    cmp ax, [block_gen_hi+di]
    jne .next_retire
    shr di, 1
    mov byte [block_state+di], 2
.next_retire:
    inc word [mount_block]
    cmp word [mount_block], 128
    jb .retire
    mov word [mount_block], 0
.erase_retired:
    mov byte [debug_stage], 4
    mov bx, [mount_block]
    cmp byte [block_state+bx], 2
    jne .next_erased
    mov [cur_block], bx
    call erase_block
    jc .bad
    mov bx, [mount_block]
    mov byte [block_state+bx], 0
    inc word [free_blocks]
.next_erased:
    inc word [mount_block]
    cmp word [mount_block], 128
    jb .erase_retired
    mov word [mount_block], 0
.records:
    mov byte [debug_stage], 5
    mov bx, [mount_block]
    cmp byte [block_state+bx], 1
    jne .next_block
    ; Select the newest block as the sole append destination.
    shl bx, 1
    mov di, [active_block]
    cmp di, 0ffffh
    je .active
    shl di, 1
    mov ax, [block_gen_hi+bx]
    cmp ax, [block_gen_hi+di]
    ja .active
    jb .load_records
    mov ax, [block_gen_lo+bx]
    cmp ax, [block_gen_lo+di]
    je .bad                  ; duplicated committed generations are invalid
    jb .load_records
.active:
    mov ax, [mount_block]
    mov [active_block], ax
.load_records:
    mov ax, [mount_block]
    mov [cur_block], ax
    mov word [cur_word], 16
    mov di, meta_buf
    mov cx, SLOTS*4
    call read_words
    mov word [mount_slot], 0
.slot:
    mov si, [mount_slot]
    shl si, 3
    add si, meta_buf
    mov ax, [si]
    and ax, [si+2]
    and ax, [si+4]
    and ax, [si+6]
    cmp ax, 0ffffh
    je .next_slot
    mov bx, [mount_block]
    shl bx, 1
    mov ax, [mount_slot]
    inc ax
    mov [block_tail+bx], ax
    cmp word [si+6], 0
    jne .next_slot
    mov ax, [si]
    cmp ax, LOGICAL_SECTORS
    jae .bad
    mov dx, ax
    xor dx, [si+2]
    cmp dx, 0ffffh
    jne .bad
    mov dx, ax
    shl dx, 1
    mov di, dx
    mov dx, [es:di]
    cmp dx, 0ffffh
    je .install
    mov di, dx
    mov cl, 8
    shr di, cl
    shl di, 1
    mov cx, [block_gen_hi+bx]
    cmp cx, [block_gen_hi+di]
    ja .install
    jb .next_slot
    mov cx, [block_gen_lo+bx]
    cmp cx, [block_gen_lo+di]
    jb .next_slot
    ; Same-block slots are scanned in ascending order.
.install:
    mov dx, [mount_block]
    mov cl, 8
    shl dx, cl
    or dx, [mount_slot]
    call map_install
.next_slot:
    inc word [mount_slot]
    cmp word [mount_slot], SLOTS
    jb .slot
.next_block:
    inc word [mount_block]
    cmp word [mount_block], 128
    jb .records
    cmp word [free_blocks], 2
    jb .bad
    mov ax, [active_block]
    inc ax
    and ax, 127
    mov [alloc_cursor], ax
    pop es
    popa
    clc
    ret
.bad:
    pop es
    popa
    jmp ftl_fail

; AX=LBA, DX=physical record identifier. Update map and live counts.
map_install:
    pusha
    push es
    mov bx, [map_segment]
    mov es, bx
    mov bx, ax
    shl bx, 1
    mov di, [es:bx]
    cmp di, 0ffffh
    je .new
    mov cl, 8
    shr di, cl
    shl di, 1
    dec word [block_live+di]
.new:
    mov [es:bx], dx
    mov di, dx
    mov cl, 8
    shr di, cl
    shl di, 1
    inc word [block_live+di]
    pop es
    popa
    ret

; Return BX=free block, preserving other registers. CF if none.
find_free:
    push cx
    mov bx, [alloc_cursor]
    mov cx, 128
.loop:
    cmp byte [block_state+bx], 0
    je .found
    inc bx
    and bx, 127
    loop .loop
    pop cx
    stc
    ret
.found:
    mov [alloc_cursor], bx
    inc word [alloc_cursor]
    and word [alloc_cursor], 127
    pop cx
    clc
    ret

; BX=new block, DX=GC source or FFFF. Erase, commit header, make active.
create_block:
    mov byte [debug_stage], 6
    pusha
    push es
    mov [create_id], bx
    mov [create_source], dx
    mov [cur_block], bx
    call erase_block
    jc .bad
    add word [max_gen_lo], 1
    adc word [max_gen_hi], 0
    mov ax, [max_gen_lo]
    or ax, [max_gen_hi]
    jz .bad
    push ds
    pop es
    mov si, header_template
    mov di, header_buf
    mov cx, 16
    rep movsw
    mov ax, [max_gen_lo]
    mov [header_buf+8], ax
    not ax
    mov [header_buf+12], ax
    mov ax, [max_gen_hi]
    mov [header_buf+10], ax
    not ax
    mov [header_buf+14], ax
    mov bx, [create_source]
    mov [header_buf+20], bx
    cmp bx, 0ffffh
    je .normal
    shl bx, 1
    mov ax, [block_gen_lo+bx]
    mov [header_buf+16], ax
    mov ax, [block_gen_hi+bx]
    mov [header_buf+18], ax
    jmp .crc
.normal:
    mov word [header_buf+16], 0
    mov word [header_buf+18], 0
    mov word [header_buf+28], 0fffeh
.crc:
    mov si, header_buf
    mov cx, 22
    call crc16
    mov [header_buf+22], ax
    not ax
    mov [header_buf+24], ax
    mov word [cur_word], 0
    mov si, header_buf
    mov cx, 15
    call program_words
    jc .bad
    mov word [cur_word], 15
    mov si, commit_zero
    mov cx, 1
    call program_words
    jc .bad
    mov bx, [create_id]
    mov byte [block_state+bx], 1
    mov [active_block], bx
    shl bx, 1
    mov ax, [max_gen_lo]
    mov [block_gen_lo+bx], ax
    mov ax, [max_gen_hi]
    mov [block_gen_hi+bx], ax
    mov word [block_tail+bx], 0
    mov word [block_live+bx], 0
    mov ax, [header_buf+16]
    mov [block_src_lo+bx], ax
    mov ax, [header_buf+18]
    mov [block_src_hi+bx], ax
    mov ax, [header_buf+20]
    mov [block_source+bx], ax
    mov ax, [header_buf+28]
    mov [block_gc_state+bx], ax
    dec word [free_blocks]
    pop es
    popa
    clc
    ret
.bad:
    pop es
    popa
    jmp ftl_fail

; AX=LBA -> sector_buf. Missing mappings read as zero sectors.
ftl_read:
    mov byte [debug_stage], 7
    pusha
    push es
    mov [read_lba], ax
    mov bx, [map_segment]
    mov es, bx
    mov bx, ax
    shl bx, 1
    mov ax, [es:bx]
    cmp ax, 0ffffh
    je .zero
    mov [read_phys], ax
    mov bx, ax
    mov cl, 8
    shr bx, cl
    mov [cur_block], bx
    and ax, 255
    shl ax, 2
    add ax, 16
    mov [cur_word], ax
    mov cx, 4
    mov di, record_buf
    call read_words
    mov ax, [read_lba]
    cmp ax, [record_buf]
    jne .bad
    not ax
    cmp ax, [record_buf+2]
    jne .bad
    cmp word [record_buf+6], 0
    jne .bad
    mov ax, [read_phys]
    and ax, 255
    mov cl, 8
    shl ax, cl
    add ax, 400h
    mov [cur_word], ax
    mov di, sector_buf
    mov cx, 256
    call read_words
    mov si, sector_buf
    mov cx, 512
    call crc16
    cmp ax, [record_buf+4]
    jne .bad
    pop es
    popa
    clc
    ret
.zero:
    push ds
    pop es
    mov di, sector_buf
    xor ax, ax
    mov cx, 256
    rep stosw
    pop es
    popa
    clc
    ret
.bad:
    mov word [flash_status], 0fffdh
    pop es
    popa
    jmp ftl_fail

; Append sector_buf under record_lba. Caller has ensured a free active slot.
append_record:
    mov byte [debug_stage], 8
    pusha
    mov bx, [active_block]
    cmp bx, 128
    jae .bad
    mov [cur_block], bx
    shl bx, 1
    mov ax, [block_tail+bx]
    cmp ax, SLOTS
    jae .bad
    mov [record_slot], ax
    inc word [block_tail+bx]
    mov ax, [record_lba]
    mov [record_buf], ax
    not ax
    mov [record_buf+2], ax
    mov si, sector_buf
    mov cx, 512
    call crc16
    mov [record_buf+4], ax
    mov word [record_buf+6], 0ffffh
    mov ax, [record_slot]
    shl ax, 2
    add ax, 16
    mov [cur_word], ax
    mov si, record_buf
    mov cx, 3
    call program_words
    jc .bad
    mov ax, [record_slot]
    mov cl, 8
    shl ax, cl
    add ax, 400h
    mov [cur_word], ax
    mov si, sector_buf
    mov bp, 16
.data:
    mov cx, 16
    call program_words
    jc .bad
    add si, 32
    add word [cur_word], 16
    dec bp
    jnz .data
    mov ax, [record_slot]
    shl ax, 2
    add ax, 19
    mov [cur_word], ax
    mov si, commit_zero
    mov cx, 1
    call program_words
    jc .bad
    mov ax, [record_lba]
    mov dx, [active_block]
    mov cl, 8
    shl dx, cl
    or dx, [record_slot]
    call map_install
    popa
    clc
    ret
.bad:
    popa
    jmp ftl_fail

ensure_space:
    mov byte [debug_stage], 9
    pusha
    cmp byte [fatal_error], 0
    jne .bad
    mov bx, [active_block]
    cmp bx, 128
    jae .allocate
    shl bx, 1
    cmp word [block_tail+bx], SLOTS
    jb .ok
.allocate:
    cmp word [free_blocks], 2
    jbe .collect
    call find_free
    jc .bad
    mov dx, 0ffffh
    call create_block
    jc .bad
    jmp .ok
.collect:
    mov word [gc_source], 0ffffh
    mov dx, SLOTS
    xor bx, bx
.choose:
    cmp byte [block_state+bx], 1
    jne .next
    mov di, bx
    shl di, 1
    cmp word [block_gc_state+di], 0fffeh
    jne .bad
    mov ax, [block_live+di]
    cmp ax, dx
    jae .next
    mov dx, ax
    mov [gc_source], bx
.next:
    inc bx
    cmp bx, 128
    jb .choose
    cmp word [gc_source], 128
    jae .bad
    call find_free
    jc .bad
    mov dx, [gc_source]
    call create_block
    jc .bad
    mov word [gc_slot], 0
.copy:
    mov ax, [gc_source]
    mov [cur_block], ax
    mov ax, [gc_slot]
    shl ax, 2
    add ax, 16
    mov [cur_word], ax
    mov di, record_buf
    mov cx, 4
    call read_words
    cmp word [record_buf+6], 0
    jne .next_copy
    mov ax, [record_buf]
    cmp ax, LOGICAL_SECTORS
    jae .bad
    mov dx, [gc_source]
    mov cl, 8
    shl dx, cl
    or dx, [gc_slot]
    push es
    mov bx, [map_segment]
    mov es, bx
    mov bx, ax
    shl bx, 1
    cmp dx, [es:bx]
    pop es
    jne .next_copy
    mov [record_lba], ax
    call ftl_read
    jc .bad
    call append_record
    jc .bad
.next_copy:
    inc word [gc_slot]
    cmp word [gc_slot], SLOTS
    jb .copy
    mov bx, [gc_source]
    shl bx, 1
    cmp word [block_live+bx], 0
    jne .bad
    mov ax, [active_block]
    mov [cur_block], ax
    mov word [cur_word], 14
    mov si, gc_complete_word
    mov cx, 1
    call program_words
    jc .bad
    mov bx, [active_block]
    shl bx, 1
    mov word [block_gc_state+bx], 0fffeh
    mov ax, [gc_source]
    mov [cur_block], ax
    call erase_block
    jc .bad
    mov bx, [gc_source]
    mov byte [block_state+bx], 0
    inc word [free_blocks]
.ok:
    popa
    clc
    ret
.bad:
    popa
    jmp ftl_fail
