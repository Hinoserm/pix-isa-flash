; Persistent setup is held in two AT29C257 64-byte pages, outside the
; declared 16 KiB option-ROM checksum range. No data-flash capacity is used.
%define CFG_PAGE_A 7f80h
%define CFG_PAGE_B 7fc0h
setup_load:
    push ds
    pop es
    mov di, saved_settings
    xor ax, ax
    mov cx, 12
    rep stosb
    ; Settings byte 5 stores a zero-based 2..20 second boot-delay index.
    ; Index 3 is the factory default: 5 seconds.
    mov byte [saved_settings+5], 3
    mov byte [cfg_valid_flag], 0
    mov word [cfg_active], 0
    mov word [cfg_gen_lo], 0
    mov word [cfg_gen_hi], 0
    mov bx, CFG_PAGE_A
    call .slot
    mov bx, CFG_PAGE_B
    call .slot
    push ds
    pop es
    mov si, saved_settings
    mov di, chosen_settings
    mov cx, 6
    rep movsb
    ret
.slot:
    pusha
    push ds
    push es
    mov si, bx
    mov ax, [oprom_segment]
    push ds
    pop es
    mov ds, ax
    mov di, cfg_buf
    mov cx, 32
    rep movsw
    pop es
    pop ds
    call config_valid
    jc .done
    mov ax, [cfg_buf+10]
    cmp ax, [cfg_gen_hi]
    ja .accept
    jb .done
    mov ax, [cfg_buf+8]
    cmp ax, [cfg_gen_lo]
    jbe .done
.accept:
    mov [cfg_active], bx
    mov ax, [cfg_buf+8]
    mov [cfg_gen_lo], ax
    mov ax, [cfg_buf+10]
    mov [cfg_gen_hi], ax
    mov al, [cfg_buf+16]
    mov [saved_drive], al
    mov ax, [cfg_buf+20]
    mov [saved_settings+1], ax
    mov ax, [cfg_buf+22]
    mov [saved_settings+3], ax
    mov al, [cfg_buf+24]
    mov [saved_settings+5], al
    mov byte [cfg_valid_flag], 1
.done:
    popa
    ret

config_valid:
    cmp word [cfg_buf], 'SL'
    jne .bad
    cmp word [cfg_buf+2], 'AT'
    jne .bad
    cmp word [cfg_buf+4], 'CF'
    jne .bad
    cmp word [cfg_buf+6], 'G1'
    jne .bad
    mov ax, [cfg_buf+8]
    or ax, [cfg_buf+10]
    jz .bad
    mov ax, [cfg_buf+8]
    xor ax, [cfg_buf+12]
    cmp ax, 0ffffh
    jne .bad
    mov ax, [cfg_buf+10]
    xor ax, [cfg_buf+14]
    cmp ax, 0ffffh
    jne .bad
    cmp byte [cfg_buf+16], 2
    ja .bad
    mov ax, [cfg_buf+16]
    xor al, ah
    cmp al, 0ffh
    jne .bad
    cmp word [cfg_buf+18], 3
    jne .bad
    cmp byte [cfg_buf+20], 1
    ja .bad
    cmp byte [cfg_buf+21], 3
    ja .bad
    cmp byte [cfg_buf+22], 7
    ja .bad
    cmp byte [cfg_buf+23], 5
    ja .bad
    cmp byte [cfg_buf+24], 18
    ja .bad
    cmp byte [cfg_buf+63], 0a5h
    jne .bad
    mov si, cfg_buf
    mov cx, 60
    call crc16
    cmp ax, [cfg_buf+60]
    jne .bad
    xor ax, ax
    mov si, cfg_buf
    mov cx, 64
.sum:
    add al, [si]
    inc si
    loop .sum
    cmp al, 0c0h
    jne .bad
    clc
    ret
.bad:
    stc
    ret

; AX=BIOS key (AH=scan for arrows), CF=1 if no input. OFF never polls UART.
setup_get_key:
    pusha
    push ds
    push es
    mov ah, 1
    int 16h
    push cs
    pop ds
    jz .serial
    xor ah, ah
    int 16h
    push cs
    pop ds
    jmp .got
.serial:
    cmp byte [serial_debug], 0
    je .none
    cmp byte [serial_enabled], 0
    je .none
    mov dx, [serial_base]
    add dx, 5
    in al, dx
    test al, 1
    jz .none
    mov dx, [serial_base]
    in al, dx
    xor ah, ah
    ; ANSI cursor sequences for an enabled serial terminal.
    cmp byte [serial_escape], 1
    je .bracket
    cmp byte [serial_escape], 2
    je .arrow
    cmp al, 27
    jne .got
    mov byte [serial_escape], 1
    jmp .none
.bracket:
    mov byte [serial_escape], 2
    cmp al, '['
    je .none
    cmp al, 'O'
    je .none
    mov byte [serial_escape], 0
    jmp .got
.arrow:
    mov byte [serial_escape], 0
    cmp al, 'A'
    jb .none
    cmp al, 'D'
    ja .none
    xor bx, bx
    mov bl, al
    sub bl, 'A'
    mov ah, [arrow_scans+bx]
    xor al, al
.got:
    mov [setup_key], ax
    pop es
    pop ds
    popa
    mov ax, [setup_key]
    clc
    ret
.none:
    pop es
    pop ds
    popa
    stc
    ret

setup_prompt:
    mov ah, 03h
    xor bx, bx
    int 10h
    mov [prompt_row], dh
    push es
    xor ax, ax
    mov es, ax
    mov ax, [es:46ch]
    mov [setup_start_tick], ax
    xor ax, ax
    mov al, [chosen_settings+5]
    add ax, 2
    mov [boot_delay_seconds], ax
    mov bx, 18
    mul bx
    inc ax
    mov [setup_timeout_ticks], ax
    mov byte [setup_second], 0ffh
    mov word [setup_watch_lo], 0
    mov word [setup_watch_hi], 512
.wait:
    mov ax, [es:46ch]
    sub ax, [setup_start_tick]
    cmp ax, [setup_timeout_ticks]
    jae .continue
    xor dx, dx
    mov bx, 18
    div bx
    mov dx, [boot_delay_seconds]
    sub dx, ax
    cmp dl, [setup_second]
    je .key
    mov [setup_second], dl
    mov ax, dx
    xor dx, dx
    mov bx, 10
    div bx
    add al, '0'
    add dl, '0'
    cmp al, '0'
    jne .store_count
    mov al, ' '
.store_count:
    mov [count_digits], al
    mov [count_digits+1], dl
    mov dh, [prompt_row]
    xor dl, dl
    call ui_cursor
    mov si, setup_prompt_msg
    call puts
.key:
    call setup_get_key
    jc .watch
    cmp al, 's'
    je .enter
    cmp al, 'S'
    je .enter
    cmp al, 13
    je .continue
    cmp al, 27
    je .continue
.watch:
    dec word [setup_watch_lo]
    jnz .wait
    dec word [setup_watch_hi]
    jnz .wait
.continue:
    pop es
    call newline
    ret
.enter:
    pop es
    jmp setup_menu

setup_menu:
    mov byte [menu_row], 0
    mov word [menu_status], menu_help
.redraw:
    call menu_draw
.wait:
    call setup_get_key
    jc .wait
    cmp al, 0
    je .extended
    cmp al, 0e0h
    je .extended
    cmp al, 9
    je .down
    cmp al, 27
    je .continue
    cmp al, 13
    je .activate
    cmp al, ' '
    je .edit_right
    cmp al, 'e'
    je .erase
    cmp al, 'E'
    je .erase
    ; In particular S is ignored: typematic setup-entry repeats are harmless.
    jmp .wait
.extended:
    cmp ah, 48h
    je .up
    cmp ah, 50h
    je .down
    cmp ah, 4bh
    je .edit_left
    cmp ah, 4dh
    je .edit_right
    jmp .wait
.up:
    cmp byte [menu_row], 0
    jne .up_one
    mov byte [menu_row], 12
.up_one:
    dec byte [menu_row]
    jmp .redraw
.down:
    inc byte [menu_row]
    cmp byte [menu_row], 12
    jb .redraw
    mov byte [menu_row], 0
    jmp .redraw
.edit_left:
    mov dl, 0ffh
    jmp .edit
.edit_right:
    mov dl, 1
.edit:
    xor bx, bx
    mov bl, [menu_row]
    cmp bl, 6
    jae .wait
    mov al, [chosen_settings+bx]
    add al, dl
    cmp al, 0ffh
    jne .upper
    mov al, [setting_max+bx]
    jmp .store
.upper:
    cmp al, [setting_max+bx]
    jbe .store
    xor al, al
.store:
.changed:
    mov [chosen_settings+bx], al
    call serial_apply
    mov word [menu_status], changed_msg
    jmp .redraw
.activate:
    mov al, [menu_row]
    cmp al, 6
    jb .edit_right
    je .save
    cmp al, 7
    je .continue
    cmp al, 8
    je .discard
    cmp al, 9
    je .stats
    cmp al, 10
    je .erase
    ; Restore defaults, remaining in setup until explicitly saved/continued.
    push ds
    pop es
    mov di, chosen_settings
    mov cx, 6
    xor ax, ax
    rep stosb
    mov byte [chosen_settings+5], 3
    call serial_apply
    mov word [menu_status], changed_msg
    jmp .redraw
.save:
    push ds
    pop es
    mov si, saved_settings
    mov di, chosen_settings
    mov cx, 6
    repe cmpsb
    jne .write
    cmp byte [cfg_valid_flag], 0
    jne .saved
.write:
    mov word [menu_status], saving_msg
    call menu_draw
    call config_save
    jc .save_failed
.saved:
    mov word [menu_status], saved_msg
    jmp .redraw
.save_failed:
    mov word [menu_status], save_failed_msg
    jmp .redraw
.discard:
    push ds
    pop es
    mov si, saved_settings
    mov di, chosen_settings
    mov cx, 6
    rep movsb
    call serial_apply
.continue:
    call clear_screen
    ret
.stats:
    call setup_stats
    jmp .redraw
.erase:
    call clear_screen
    call erase_menu_disk
    jc .return
    cmp byte [erase_reboot], 0
    je .redraw
.return:
    ret

; Apply only to resident variables. OFF performs no UART cleanup/probing.
serial_apply:
    pusha
    mov al, [chosen_settings+1]
    mov [serial_debug], al
    mov byte [serial_enabled], 0
    xor bx, bx
    mov bl, [chosen_settings+2]
    shl bx, 1
    mov ax, [port_values+bx]
    mov [serial_base], ax
    mov bl, [chosen_settings+3]
    xor bh, bh
    shl bx, 1
    mov ax, [baud_values+bx]
    mov [serial_divisor], ax
    mov bl, [chosen_settings+4]
    xor bh, bh
    mov al, [format_values+bx]
    mov [serial_lcr], al
    call serial_claim
    popa
    ret

; Text UI: BIOS screen services only; no direct UART calls.
ui_cursor:
    pushf
    pusha
    push ds
    push es
    mov ah, 02h
    xor bx, bx
    int 10h
    pop es
    pop ds
    popa
    popf
    ret
ui_text:
    ; SI=text, BL=attribute. Cursor advance without scrolling/teletype quirks.
    pushf
    pusha
    push ds
    push es
    cld
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 09h
    xor bh, bh
    mov cx, 1
    push si
    int 10h
    mov ah, 03h
    int 10h
    inc dl
    mov ah, 02h
    int 10h
    pop si
    jmp .loop
.done:
    pop es
    pop ds
    popa
    popf
    ret
menu_draw:
    pusha
    push ds
    push es
    mov ax, 0600h
    mov bh, 17h
    xor cx, cx
    mov dx, 184fh
    int 10h
    mov dx, 0104h
    call ui_cursor
    mov bl, 1fh
    mov si, menu_title
    call ui_text
    mov dx, 0204h
    call ui_cursor
    mov bl, 17h
    mov si, menu_subtitle
    call ui_text
    xor bp, bp
.row:
    mov dx, bp
    add dh, 0 ; row is calculated from BP below
    mov dh, dl
    add dh, 5
    mov dl, 4
    call ui_cursor
    mov bl, 17h
    mov ax, bp
    cmp al, [menu_row]
    jne .label
    mov bl, 70h
.label:
    mov si, bp
    shl si, 1
    mov si, [menu_labels+si]
    call ui_text
    cmp bp, 6
    jae .next
    mov dx, bp
    mov dh, dl
    add dh, 5
    mov dl, 36
    call ui_cursor
    mov si, bp
    mov al, [chosen_settings+si]
    xor ah, ah
    shl ax, 1
    shl si, 1
    mov si, [value_tables+si]
    add si, ax
    mov si, [si]
    call ui_text
.next:
    inc bp
    cmp bp, 12
    jb .row
    mov dx, 1104h
    call ui_cursor
    mov bl, 1eh
    mov si, [menu_status]
    call ui_text
    mov dx, 1404h
    call ui_cursor
    mov bl, 17h
    mov si, menu_keys
    call ui_text
    mov dx, 1604h
    call ui_cursor
    mov si, menu_footer
    call ui_text
    pop es
    pop ds
    popa
    ret

apply_drive_choice:
    mov al, [chosen_drive]
    add al, 80h
    mov [virtual_drive], al
    ret

; Read-only latch of PIT channel 0. No IRQs, BIOS, or DOS are needed.
; Stock DOS timer: mode 3 decrements twice per 1.193182 MHz cycle, so a
; total 65535 count decrement takes >=27ms (mode 2 takes >=54ms).
; This exceeds AT29C257's 10ms ID/page interval. A stopped timer fails.
rom_pause:
    pushf
    pusha
    call rom_pit_sample
    mov bx, ax
    mov bp, 0ffffh
    xor si, si
    mov di, 128
.loop:
    call rom_pit_sample
    mov dx, bx
    sub dx, ax
    mov bx, ax
    sub bp, dx
    jbe .ok
    dec si
    jnz .loop
    dec di
    jnz .loop
    popa
    popf
    stc
    ret
.ok:
    popa
    popf
    clc
    ret
rom_pit_sample:
    xor al, al
    out 43h, al
    in al, 40h
    mov ah, al
    in al, 40h
    xchg al, ah
    ret

; Executes only from the relocated RAM core. No chip erase command is used.
; Returns CF=0 only for confirmed 1F/DC and restored read-array mode.
oprom_identify:
    pushf
    cli
    push es
    mov ax, [oprom_segment]
    mov es, ax
    mov word [oprom_id], 0ffffh
    cmp word [es:0], 0aa55h
    jne .bad
    cmp byte [es:2], 32
    jne .bad
    pushf
    cli
    mov byte [es:5555h], 0aah
    mov byte [es:2aaah], 55h
    mov byte [es:5555h], 90h
    popf
    call rom_pause
    mov al, [es:0]
    mov ah, [es:1]
    mov [oprom_id], ax
    pushf
    cli
    mov byte [es:5555h], 0aah
    mov byte [es:2aaah], 55h
    mov byte [es:5555h], 0f0h
    popf
    call rom_pause
    cmp word [oprom_id], 0dc1fh
    jne .bad
    cmp word [es:0], 0aa55h
    jne .bad
    pop es
    popf
    clc
    ret
.bad:
    pop es
    popf
    stc
    ret

config_save:
    pushf
    pusha
    push es
    call oprom_identify
    jc .bad
    push ds
    pop es
    mov di, cfg_buf
    mov ax, 0ffffh
    mov cx, 32
    rep stosw
    mov si, cfg_magic
    mov di, cfg_buf
    mov cx, 4
    rep movsw
    mov ax, [cfg_gen_lo]
    mov dx, [cfg_gen_hi]
    add ax, 1
    adc dx, 0
    mov cx, ax
    or cx, dx
    jz .bad
    mov [cfg_buf+8], ax
    mov [cfg_buf+10], dx
    not ax
    not dx
    mov [cfg_buf+12], ax
    mov [cfg_buf+14], dx
    mov al, [chosen_drive]
    mov [cfg_buf+16], al
    not al
    mov [cfg_buf+17], al
    mov word [cfg_buf+18], 3
    mov si, chosen_settings+1
    mov di, cfg_buf+20
    mov cx, 5
    rep movsb
    mov si, cfg_buf
    mov cx, 60
    call crc16
    mov [cfg_buf+60], ax
    mov byte [cfg_buf+62], 0
    mov byte [cfg_buf+63], 0a5h
    xor ax, ax
    mov si, cfg_buf
    mov cx, 64
.sum:
    add al, [si]
    inc si
    loop .sum
    neg al
    add al, 0c0h
    mov [cfg_buf+62], al
    mov di, CFG_PAGE_A
    cmp word [cfg_active], CFG_PAGE_A
    jne .target
    mov di, CFG_PAGE_B
.target:
    cli
    mov [cfg_target], di
    mov ax, [oprom_segment]
    mov es, ax
    mov si, cfg_buf
    mov cx, 64
    ; Load the entire 64-byte page without interrupt-induced byte gaps.
    pushf
    cli
    mov byte [es:5555h], 0aah
    mov byte [es:2aaah], 55h
    mov byte [es:5555h], 0a0h
    rep movsb
    popf
    call rom_pause
    mov word [cfg_retries], 20
.verify:
    mov di, [cfg_target]
    mov si, cfg_buf
    mov cx, 64
    repe cmpsb
    je .success
    call rom_pause
    dec word [cfg_retries]
    jnz .verify
    jmp .bad
.success:
    mov ax, [cfg_target]
    mov [cfg_active], ax
    mov ax, [cfg_buf+8]
    mov [cfg_gen_lo], ax
    mov ax, [cfg_buf+10]
    mov [cfg_gen_hi], ax
    push ds
    pop es
    mov si, chosen_settings
    mov di, saved_settings
    mov cx, 6
    rep movsb
    mov byte [cfg_valid_flag], 1
    pop es
    popa
    popf
    clc
    ret
.bad:
    pop es
    popa
    popf
    stc
    ret

align 2
cfg_magic db 'SLATCFG1'
cfg_buf times 64 db 0
cfg_gen_lo dw 0
cfg_gen_hi dw 0
cfg_active dw 0
cfg_target dw 0
cfg_retries dw 0
oprom_id dw 0ffffh
setup_start_tick dw 0
setup_watch_lo dw 0
setup_watch_hi dw 0
saved_settings times 6 db 0
chosen_settings times 6 db 0
saved_drive equ saved_settings
chosen_drive equ chosen_settings
cfg_valid_flag db 0
setup_key dw 0
setup_second db 0
boot_delay_seconds dw 5
setup_timeout_ticks dw 91
prompt_row db 0
serial_escape db 0
menu_row db 0
menu_status dw menu_help
arrow_scans db 48h,50h,4dh,4bh
setting_max db 2,1,3,7,5,18
port_values dw 03f8h,02f8h,03e8h,02e8h
baud_values dw 1,2,3,6,12,24,48,96
format_values db 3,1ah,0ah,1bh,0bh,7
setup_prompt_msg db 'Press S for Setup | Enter to boot | Continuing in '
count_digits db ' 5'
    db ' seconds...',0
menu_title db 'SLATON COMPUTERS  |  16MB ISA FLASH',0
menu_subtitle db 'Firmware Setup                  15.5 MiB disk / 512 KiB reserve',0
menu_help db 'Select an item with Up/Down. Enter activates the selected item.',0
changed_msg db 'Unsaved changes. Continue Boot uses them for this boot only.',0
menu_keys db 'Up/Down: select   Left/Right: change   Enter: activate',0
menu_footer db 'Esc: continue without saving   Serial flow control: none',0
menu_labels dw label_drive,label_debug,label_port,label_baud,label_format,label_delay
            dw label_save,label_boot,label_discard,label_stats,label_erase,label_defaults
label_drive db '  Boot drive                   ',0
label_debug db '  Serial debugging             ',0
label_port db '  Serial port                  ',0
label_baud db '  Baud rate                    ',0
label_format db '  Data / parity / stop bits    ',0
label_delay db '  Boot delay                   ',0
label_save db '  Save settings                ',0
label_boot db '  Continue boot                ',0
label_discard db '  Discard changes and continue ',0
label_stats db '  Flash usage and metadata...  ',0
label_erase db '  Erase flash disk...          ',0
label_defaults db '  Restore defaults             ',0
value_tables dw drive_texts,debug_texts,port_texts,baud_texts,format_texts,delay_texts
drive_texts dw val_80h,val_81h,val_disabled
debug_texts dw val_off,val_on
port_texts dw val_com1,val_com2,val_com3,val_com4
baud_texts dw val_115200,val_57600,val_38400,val_19200,val_9600,val_4800,val_2400,val_1200
format_texts dw val_8n1,val_7e1,val_7o1,val_8e1,val_8o1,val_8n2
delay_texts dw val_2s,val_3s,val_4s,val_5s,val_6s,val_7s,val_8s,val_9s,val_10s
            dw val_11s,val_12s,val_13s,val_14s,val_15s,val_16s,val_17s,val_18s,val_19s,val_20s
val_80h db 'C: target (BIOS drive 80h)',0
val_81h db 'D: target (BIOS drive 81h)',0
val_disabled db 'None (disk disabled)',0
val_off db 'OFF - no serial port access',0
val_on db 'ON',0
val_com1 db 'COM1 - 03F8h',0
val_com2 db 'COM2 - 02F8h',0
val_com3 db 'COM3 - 03E8h',0
val_com4 db 'COM4 - 02E8h',0
val_115200 db '115200',0
val_57600 db '57600',0
val_38400 db '38400',0
val_19200 db '19200',0
val_9600 db '9600',0
val_4800 db '4800',0
val_2400 db '2400',0
val_1200 db '1200',0
val_8n1 db '8N1',0
val_7e1 db '7E1',0
val_7o1 db '7O1',0
val_8e1 db '8E1',0
val_8o1 db '8O1',0
val_8n2 db '8N2',0
val_2s db '2 seconds',0
val_3s db '3 seconds',0
val_4s db '4 seconds',0
val_5s db '5 seconds',0
val_6s db '6 seconds',0
val_7s db '7 seconds',0
val_8s db '8 seconds',0
val_9s db '9 seconds',0
val_10s db '10 seconds',0
val_11s db '11 seconds',0
val_12s db '12 seconds',0
val_13s db '13 seconds',0
val_14s db '14 seconds',0
val_15s db '15 seconds',0
val_16s db '16 seconds',0
val_17s db '17 seconds',0
val_18s db '18 seconds',0
val_19s db '19 seconds',0
val_20s db '20 seconds',0
saving_msg db 'Saving settings to OPROM...',0
saved_msg db 'Settings saved. Select Continue Boot when ready.',0
save_failed_msg db 'NOT SAVED. Check OPROM shadowing/write protection.',0

; Invoked only before INT13 installation. Never modifies the AT29C257.

erase_menu_disk:
    call erase_window
    mov dx, 0a0eh
    call ui_cursor
    mov bl, 70h
    mov si, erase_warning
    call ui_text
    mov dx, 0c0eh
    call ui_cursor
    mov si, erase_quick_hint
    call ui_text
    mov dx, 0d0eh
    call ui_cursor
    mov si, erase_full_hint
    call ui_text
    mov dx, 100eh
    call ui_cursor
    mov si, erase_cancel_hint
    call ui_text
.confirm:
    call setup_get_key
    jc .confirm
    mov byte [erase_full], 0
    cmp al, 13
    je .begin
    and al, 0dfh
    cmp al, 'F'
    jne .cancel
    mov byte [erase_full], 1
.begin:
    mov word [cur_block], 0
    mov word [cur_word], 0
    call erase_window
    mov dx, 0a0eh
    call ui_cursor
    mov bl, 70h
    mov si, erase_quick_mode
    cmp byte [erase_full], 0
    je .mode
    mov si, erase_full_mode
.mode:
    call ui_text
    mov dx, 100eh
    call ui_cursor
    mov si, erase_start
    call ui_text
    call erase_progress
.loop:
    cmp byte [erase_full], 0
    je .quick
    call erase_block
    jmp .result
.quick:
    ; Invalidate magic by clearing its first word; data payload is untouched.
    mov si, commit_zero
    mov cx, 1
    call program_words
.result:
    jc .failed
    inc word [cur_block]
    call erase_progress
    cmp word [cur_block], 128
    jb .loop
    mov byte [erase_reboot], 1
    mov dx, 100eh
    call ui_cursor
    mov bl, 72h
    mov si, erase_ok
    call ui_text
    mov dx, 1400h
    call ui_cursor
    clc
    ret
.cancel:
    clc
    ret
.failed:
    mov byte [fatal_error], 1
    mov dx, 100eh
    call ui_cursor
    mov bl, 74h
    mov si, erase_failed_msg
    call ui_text
    mov dx, 1400h
    call ui_cursor
    stc
    ret
erase_full db 0
erase_reboot db 0
; Centered 60-column panel; BIOS text calls, never direct serial access.
erase_window:
    pusha
    push ds
    push es
    mov ax, 0600h
    mov bh, 17h
    xor cx, cx
    mov dx, 184fh
    int 10h
    mov ax, 0600h
    mov bh, 70h
    mov cx, 060ah
    mov dx, 1245h
    int 10h
    mov bl, 70h
    mov dx, 060ah
    call ui_cursor
    mov si, erase_border
    call ui_text
    mov dx, 120ah
    call ui_cursor
    call ui_text
    mov bp, 7
.sides:
    mov dx, bp
    mov dh, dl
    mov dl, 10
    call ui_cursor
    mov si, erase_side
    call ui_text
    mov dl, 69
    call ui_cursor
    call ui_text
    inc bp
    cmp bp, 18
    jb .sides
    mov dx, 081ch
    call ui_cursor
    mov bl, 71h
    mov si, erase_title
    call ui_text
    pop es
    pop ds
    popa
    ret

erase_progress:
    pushf
    pusha
    mov ax, [cur_block]
    mov di, erase_done_digits
    call erase_decimal
    mov ax, [cur_block]
    mov bx, 100
    mul bx
    mov bx, 128
    div bx
    mov di, erase_percent_digits
    call erase_decimal
    mov ax, [cur_block]
    mov bx, 40
    mul bx
    mov bx, 128
    div bx
    xor bx, bx
    mov di, erase_bar+1
    mov cx, 40
.fill:
    mov dl, 0b0h
    cmp bx, ax
    jae .store
    mov dl, 0dbh
.store:
    mov [di], dl
    inc di
    inc bx
    loop .fill
    mov dx, 0d13h
    call ui_cursor
    mov bl, 72h
    mov si, erase_bar
    call ui_text
    mov dx, 0e14h
    call ui_cursor
    mov bl, 70h
    mov si, erase_done_digits
    call ui_text
    popa
    popf
    ret

; Render unsigned 0..999 right-aligned into DS:DI, preserving registers.
erase_decimal:
    pusha
    xor dx, dx
    mov bx, 100
    div bx
    add al, '0'
    mov [di], al
    mov ax, dx
    xor dx, dx
    mov bx, 10
    div bx
    add al, '0'
    add dl, '0'
    mov [di+1], al
    mov [di+2], dl
    cmp byte [di], '0'
    jne .done
    mov byte [di], ' '
    cmp byte [di+1], '0'
    jne .done
    mov byte [di+1], ' '
.done:
    popa
    ret

erase_title db 'ERASE FLASH DISK',0
erase_border db '+',58 dup('-'),'+',0
erase_side db '|',0
erase_warning db 'All partitions and files will be removed.',0
erase_quick_hint db '[ Enter ]  Quick erase - metadata only',0
erase_full_hint db '[   F   ]  Full erase - both flash chips',0
erase_cancel_hint db 'Any other key cancels. OPROM settings are kept.',0
erase_quick_mode db 'Quick erase - invalidating block metadata',0
erase_full_mode db 'Full erase - erasing both flash chips',0
erase_start db 'Working... please keep the power on.           ',0
erase_ok db 'Complete. Reboot to initialize the empty disk. ',0
erase_failed_msg db 'FAILED. Disk not installed; repeat erase.      ',0
erase_bar db '[',40 dup(0b0h),']',0
erase_done_digits db '  0 / 128 blocks              '
erase_percent_digits db '  0%',0

setup_stats:
.refresh:
    call clear_screen
    mov dx, 0104h
    call ui_cursor
    mov bl, 0fh
    mov si, stats_title
    call ui_text
    mov dx, 0204h
    call ui_cursor
    mov bl, 07h
    mov si, stats_scanning
    call ui_text
    call stats_scan
    jc .failed
    xor bp, bp
.row:
    mov dx, bp
    mov dh, dl
    add dh, 4
    mov dl, 4
    call ui_cursor
    mov bl, 07h
    mov si, bp
    shl si, 1
    mov si, [st_labels+si]
    call ui_text
    mov si, bp
    shl si, 1
    mov ax, [st_counts+si]
    call stats_decimal
    mov dx, bp
    mov dh, dl
    add dh, 4
    mov dl, 56
    call ui_cursor
    mov bl, 0fh
    mov si, stats_number
    call ui_text
    inc bp
    cmp bp, 14
    jb .row
    mov dx, 1304h
    call ui_cursor
    mov bl, 07h
    mov si, stats_note1
    call ui_text
    mov dx, 1404h
    call ui_cursor
    mov si, stats_note2
    call ui_text
    mov dx, 1504h
    call ui_cursor
    mov si, stats_note3
    call ui_text
    jmp .footer
.failed:
    mov dx, 0504h
    call ui_cursor
    mov bl, 0ch
    mov si, stats_error
    call ui_text
.footer:
    mov dx, 1704h
    call ui_cursor
    mov bl, 0fh
    mov si, stats_keys
    call ui_text
.wait:
    call setup_get_key
    jc .wait
    cmp al, 'r'
    je .refresh
    cmp al, 'R'
    je .refresh
    cmp al, 27
    je .done
    cmp al, 13
    jne .wait
.done:
    ret
stats_decimal:
    pusha
    mov di, stats_number+4
    mov cx, 5
    mov bx, 10
.digit:
    xor dx, dx
    div bx
    add dl, '0'
    mov [di], dl
    dec di
    loop .digit
    mov di, stats_number
    mov cx, 4
.trim:
    cmp byte [di], '0'
    jne .done
    mov byte [di], ' '
    inc di
    loop .trim
.done:
    popa
    ret
stats_number db '00000',0
stats_title db 'SLATON FLASH USAGE  |  128 blocks x 128 KiB',0
stats_scanning db 'Read-only metadata scan; no erasing or programming.',0
stats_error db 'Both Intel flash chips could not be identified.',0
stats_note1 db 'Counts describe flash allocation, not DOS free filesystem space.',0
stats_note2 db 'Blank headers do not prove erased payloads. Reuse erases a block.',0
stats_note3 db 'Record payload CRCs are not checked. Pending copies need recovery.',0
stats_keys db 'R: refresh   Enter / Esc: return to setup',0
%include "tools/flashstats.inc"
