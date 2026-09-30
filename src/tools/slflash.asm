; DOS 3+ real-mode AT29C257 updater. 286 instruction set. No DOS/disk calls
; while the chip is in ID/program mode. Complete image is resident in RAM.
CPU 286
BITS 16
ORG 100h
start:
    cld
    push cs
    pop ds
    push cs
    pop es
    cli
    mov ax, cs
    mov ss, ax
    mov sp, stack_top
    sti
    mov dx, intro
    call print
    ; Help and missing-image handling do not depend on the programming
    ; environment or the 96 KiB work-buffer requirement.
    xor cx, cx
    mov cl, [80h]
    mov si, 81h
.help_skip:
    test cx, cx
    jnz .help_character
    jmp usage_error
.help_character:
    lodsb
    dec cx
    cmp al, ' '
    je .help_skip
    cmp al, 9
    je .help_skip
    cmp al, '/'
    jne .not_help
    test cx, cx
    jz .not_help
    lodsb
    dec cx
    cmp al, '?'
    jne .not_help
.help_tail:
    test cx, cx
    jz .show_help
    lodsb
    dec cx
    cmp al, ' '
    je .help_tail
    cmp al, 9
    je .help_tail
    jmp usage_error
.show_help:
    mov dx, usage_msg
    call print
    mov ax, 4c00h
    int 21h
.not_help:
    smsw ax
    test ax, 1
    jnz protected_error
    ; COM owns its DOS allocation. Reserve a second segment 64 KiB above
    ; the PSP for the backup, requiring 96 KiB in the allocated DOS block.
    mov ax, [2]
    mov bx, cs
    sub ax, bx
    cmp ax, 1800h
    jb memory_error
    add bx, 1000h
    jc memory_error
    mov [backup_seg], bx
    ; One unquoted DOS pathname, optionally followed by a four-digit segment.
    xor cx, cx
    mov cl, [80h]
    mov si, 81h
.skip:
    test cx, cx
    jz usage_error
    lodsb
    dec cx
    cmp al, ' '
    je .skip
    cmp al, 9
    je .skip
    mov di, filename
.name:
    cmp al, ' '
    je .tail
    cmp al, 9
    je .tail
    cmp al, '"'
    je usage_error
    stosb
    test cx, cx
    jz .name_end
    lodsb
    dec cx
    jmp .name
.tail:
    ; Parse remaining whitespace-separated tokens in any order: a four-digit
    ; segment, /NOBACKUP, and /RESETCFG.
.option_next:
    test cx, cx
    jz .name_end
    lodsb
    dec cx
    cmp al, ' '
    je .option_next
    cmp al, 9
    je .option_next
    mov di, option_buffer
    xor bx, bx
.option_copy:
    cmp al, ' '
    je .option_ready
    cmp al, 9
    je .option_ready
    cmp al, 'a'
    jb .option_store
    cmp al, 'z'
    ja .option_store
    and al, 0dfh
.option_store:
    cmp bx, 15
    jae usage_error
    stosb
    inc bx
    jcxz .option_ready
    lodsb
    dec cx
    jmp .option_copy
.option_ready:
    xor al, al
    stosb
    mov [option_length], bl
    mov [option_remaining], cx
    cmp byte [option_buffer], '/'
    jne .segment_token
    cmp bl, option_nobackup_end-option_nobackup-1
    jne .try_resetcfg
    mov si, option_buffer
    mov di, option_nobackup
    mov cx, option_nobackup_end-option_nobackup
    repe cmpsb
    jne .try_resetcfg
    mov byte [skip_backup], 1
    jmp .option_resume
.try_resetcfg:
    mov bl, [option_length]
    cmp bl, option_resetcfg_end-option_resetcfg-1
    jne usage_error
    mov si, option_buffer
    mov di, option_resetcfg
    mov cx, option_resetcfg_end-option_resetcfg
    repe cmpsb
    jne usage_error
    mov byte [reset_config], 1
    jmp .option_resume
.segment_token:
    cmp bl, 4
    jne usage_error
    cmp word [rom_override], 0
    jne usage_error
    mov si, option_buffer
    xor bx, bx
    mov bp, 4
.segment_digit:
    lodsb
    cmp al, '0'
    jb usage_error
    cmp al, '9'
    jbe .decimal
    cmp al, 'A'
    jb usage_error
    cmp al, 'F'
    ja usage_error
    sub al, 'A'-10
    jmp .digit_ready
.decimal:
    sub al, '0'
.digit_ready:
    shl bx, 4
    xor ah, ah
    or bx, ax
    dec bp
    jnz .segment_digit
    cmp bx, 0c000h
    jb usage_error
    cmp bx, 0e800h
    ja usage_error
    test bx, 7fh
    jnz usage_error
    mov [rom_override], bx
.option_resume:
    ; CX still contains the unconsumed DOS command-line byte count.
    mov cx, [option_remaining]
    jmp .option_next
.name_end:
    xor al, al
    stosb
    mov dx, filename
    mov ax, 3d00h
    int 21h
    jc file_error
    mov [handle], ax
    mov bx, ax
    mov dx, image
    mov cx, 32768
    mov ah, 3fh
    int 21h
    jc .read_bad
    cmp ax, 32768
    jne .read_bad
    mov bx, [handle]
    mov dx, extra
    mov cx, 1
    mov ah, 3fh
    int 21h
    jc .read_bad
    test ax, ax
    jnz .read_bad
    mov bx, [handle]
    mov ah, 3eh
    int 21h
    jc file_error
    cmp word [image], 0aa55h
    jne image_error
    cmp byte [image+2], 32
    jne image_error
    mov si, brand
    mov di, image+5
    mov cx, brand_end-brand
    repe cmpsb
    jne image_error
    mov si, image
    mov cx, 16384
    call sum_bytes
    test al, al
    jnz image_error
    mov si, image
    mov cx, 32768
    call sum_bytes
    test al, al
    jnz image_error
    ; Read-only scan, 2 KiB boundaries, allowing the complete 32 KiB window
    ; below the motherboard BIOS. Refuse ambiguity instead of guessing.
    mov ax, [rom_override]
    test ax, ax
    jnz .use_segment
    mov bx, 0c000h
.scan:
    mov es, bx
    cmp word [es:0], 0aa55h
    jne .next
    cmp byte [es:2], 32
    jne .next
    mov si, brand
    mov di, 5
    mov cx, brand_end-brand
    repe cmpsb
    jne .next
    inc byte [matches]
    mov [rom_seg], bx
.next:
    add bx, 80h
    cmp bx, 0e800h
    jbe .scan
    push ds
    pop es
    cmp byte [matches], 1
    je .selected
    cmp byte [matches], 0
    jne find_error
    ; A blank/unrecognized ROM has no discoverable header. Use the board's
    ; default window; the command line can select a different mapped window.
    mov ax, 0d800h
    mov dx, default_msg
    call print
.use_segment:
    mov [rom_seg], ax
.selected:
    ; Report all vector addresses within the chip, including segment aliases.
    ; Some are data pointers. User explicitly permits programming anyway.
    xor ax, ax
    mov es, ax
    xor si, si
    mov cx, 256
.vectors:
    mov ax, [es:si]
    shr ax, 4
    add ax, [es:si+2]
    jc .next_vector
    sub ax, [rom_seg]
    cmp ax, 800h
    jae .next_vector
    mov dx, vector_notice
    call print
    mov ax, si
    shr ax, 2
    call hexword
    mov dx, vector_at
    call print
    mov ax, [es:si+2]
    call hexword
    mov dl, ':'
    mov ah, 02h
    int 21h
    mov ax, [es:si]
    call hexword
    mov dx, newline
    call print
.next_vector:
    add si, 4
    loop .vectors
    push ds
    pop es
    mov dx, target_msg
    call print
    mov ax, [rom_seg]
    call hexword
    mov dx, installed_msg
    call print
    mov ax, [rom_seg]
    mov es, ax
    cmp word [es:0], 0aa55h
    jne .unknown_installed
    cmp byte [es:2], 32
    jne .unknown_installed
    mov si, brand
    mov di, 5
    mov cx, brand_end-brand
    repe cmpsb
    jne .unknown_installed
    mov byte [known_card], 1
    xor di, di
    call find_version
    call print
    call settings_v3_present
    jc .image_version
    cmp byte [reset_config], 0
    jne .image_version
    mov byte [preserve_settings], 1
    jmp .image_version
.unknown_installed:
    mov dx, unknown_msg
    call print
.image_version:
    mov dx, image_version_msg
    call print
    push ds
    pop es
    mov di, image
    call find_version
    call print
    mov dx, reset_config_msg
    cmp byte [reset_config], 0
    jne .settings_message
    mov dx, retain_msg
    cmp byte [preserve_settings], 0
    jne .settings_message
    mov dx, defaults_msg
.settings_message:
    call print
    cmp byte [skip_backup], 0
    je .backup_option_done
    mov dx, no_backup_warning
    call print
.backup_option_done:
    mov dx, confirm_msg
    call print
    mov ah, 08h
    int 21h
    and al, 0dfh
    cmp al, 'Y'
    jne cancelled
    mov dx, newline
    call print
    ; Snapshot full existing ROM in RAM; preserve both setup pages verbatim.
    push ds
    mov ax, [backup_seg]
    mov es, ax
    mov ax, [rom_seg]
    mov ds, ax
    xor si, si
    xor di, di
    mov cx, 16384
    rep movsw
    pop ds
    push ds
    pop es
    cmp byte [preserve_settings], 0
    je .settings_ready
    push ds
    mov ax, [backup_seg]
    push cs
    pop es
    mov ds, ax
    mov si, 7f80h
    mov di, image+7f80h
    mov cx, 128
    rep movsb
    pop ds
.settings_ready:
    ; Exclusive numbered backup; never overwrite or refuse merely because
    ; the backup from a previous update exists.
    cmp byte [skip_backup], 0
    jne .backup_skipped
.backup_create:
    mov bx, [backup_index]
    mov di, backup_name+4
    mov cx, 4
.backup_digits:
    rol bx, 4
    mov al, bl
    and al, 15
    add al, '0'
    cmp al, '9'
    jbe .backup_digit
    add al, 7
.backup_digit:
    stosb
    loop .backup_digits
    mov dx, backup_name
    xor cx, cx
    mov ah, 5bh
    int 21h
    jnc .backup_created
    cmp ax, 80
    jne backup_error
    inc word [backup_index]
    jnz .backup_create
    jmp backup_error
.backup_created:
    mov [handle], ax
    mov bx, ax
    push ds
    mov ax, [backup_seg]
    mov ds, ax
    xor dx, dx
    mov cx, 32768
    mov ah, 40h
    int 21h
    pop ds
    jc .backup_bad
    cmp ax, 32768
    jne .backup_bad
    mov bx, [handle]
    mov ah, 3eh
    int 21h
    jc backup_error
    mov byte [backup_name+12], '$'
    mov dx, backup_name
    call print
    mov byte [backup_name+12], 0
    mov dx, backup_ok
    call print
    jmp .identify
.backup_skipped:
    mov dx, backup_skipped_msg
    call print
.identify:
    ; Ensure timer advances before changing chip state.
    call pause
    jc clock_error
    mov dx, writing_msg
    call print
    ; Entire chip-access window runs from RAM with maskable IRQs disabled.
    ; No DOS/BIOS calls until the image has been verified or we abort.
    pushf
    pop word [saved_flags]
    cli
    mov byte [window_active], 1
    ; No return to DOS after this point. Redirect every IVT reference into
    ; the physical ROM to a RAM stub while the chip is unavailable. Normal
    ; maskable IRQs are already disabled; this also covers an NMI vector
    ; that directly points into the chip. Reset restores the IVT afterward.
    xor ax, ax
    mov es, ax
    xor si, si
    mov cx, 256
.redirect:
    mov ax, [es:si]
    shr ax, 4
    add ax, [es:si+2]
    jc .next_redirect
    sub ax, [rom_seg]
    cmp ax, 800h
    jae .next_redirect
    mov word [es:si], flash_vector_stub
    mov [es:si+2], cs
.next_redirect:
    add si, 4
    loop .redirect
    mov ax, [rom_seg]
    mov es, ax
    mov byte [es:5555h], 0aah
    mov byte [es:2aaah], 55h
    mov byte [es:5555h], 90h
    call pause
    mov al, [es:0]
    mov ah, [es:1]
    mov [chip_id], ax
    pushf
    cli
    mov byte [es:5555h], 0aah
    mov byte [es:2aaah], 55h
    mov byte [es:5555h], 0f0h
    popf
    call pause
    jc clock_error
    cmp word [chip_id], 0dc1fh
    jne id_error
    ; Array must match the snapshot after leaving software ID mode.
    push ds
    mov ax, [backup_seg]
    mov ds, ax
    xor si, si
    xor di, di
    mov cx, 32768
    repe cmpsb
    pop ds
    jne mapping_error
    ; Program offsets 0040..7FFF first, entry page 0000 last. This is NOT
    ; an atomic upgrade; interrupted programming can still require a burner.
    mov word [page], 40h
.page:
    mov ax, [rom_seg]
    mov es, ax
    mov di, [page]
    mov si, image
    add si, di
    mov cx, 64
    repe cmpsb
    je .next_page
    mov di, [page]
    mov si, image
    add si, di
    mov cx, 64
    pushf
    cli
    mov byte [es:5555h], 0aah
    mov byte [es:2aaah], 55h
    mov byte [es:5555h], 0a0h
    rep movsb
    popf
    mov bp, 20
.verify_page:
    call pause
    jc write_error
    mov di, [page]
    mov si, image
    add si, di
    mov cx, 64
    repe cmpsb
    je .next_page
    dec bp
    jnz .verify_page
    jmp write_error
.next_page:
    cmp word [page], 0
    je .verify_all
    add word [page], 64
    cmp word [page], 8000h
    jb .progress
    mov word [page], 0
    jmp .page
.progress:
    test word [page], 3ffh
    jnz .page
    jmp .page
.verify_all:
    mov ax, [rom_seg]
    mov es, ax
    xor di, di
    mov si, image
    mov cx, 32768
    repe cmpsb
    jne write_error
    mov dx, success_msg
    jmp reset_screen
.read_bad:
    mov bx, [handle]
    mov ah, 3eh
    int 21h
    jmp file_error
.backup_bad:
    mov bx, [handle]
    mov ah, 3eh
    int 21h
    jmp backup_error

; ES:DI = start of a 32 KiB image. Return DS:DX = bounded version or UNKNOWN.
; Image contents are never executed or used as pointers.
find_version:
    pusha
    mov bx, di
    mov bp, 32768-(version_prefix_end-version_prefix)-16
.candidate:
    mov di, bx
    mov si, version_prefix
    mov cx, version_prefix_end-version_prefix
    repe cmpsb
    je .matched
    inc bx
    dec bp
    jnz .candidate
.unknown:
    mov word [version_result], unknown_msg
    jmp .done
.matched:
    mov al, [es:di]
    cmp al, '0'
    jb .unknown
    cmp al, '9'
    ja .unknown
    mov si, version_buffer
    mov cx, 15
.copy:
    mov al, [es:di]
    cmp al, '.'
    je .store
    cmp al, '0'
    jb .finish
    cmp al, '9'
    ja .finish
.store:
    mov [si], al
    inc si
    inc di
    loop .copy
.finish:
    mov byte [si], '$'
    mov word [version_result], version_buffer
.done:
    popa
    mov dx, [version_result]
    ret

; ES points to the installed 32 KiB ROM. CF=0 when either redundant settings
; page passes the complete current-layout validation. Other versions are not
; migrated or copied into the incoming image.
settings_v3_present:
    push di
    mov di, 7f80h
    call settings_page_v3
    jnc .done
    mov di, 7fc0h
    call settings_page_v3
.done:
    pop di
    ret

settings_page_v3:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    cmp word [es:di], 'SL'
    jne .bad
    cmp word [es:di+2], 'AT'
    jne .bad
    cmp word [es:di+4], 'CF'
    jne .bad
    cmp word [es:di+6], 'G1'
    jne .bad
    mov ax, [es:di+8]
    mov dx, [es:di+10]
    mov bx, ax
    or bx, dx
    jz .bad
    xor ax, [es:di+12]
    xor dx, [es:di+14]
    and ax, dx
    cmp ax, 0ffffh
    jne .bad
    cmp byte [es:di+16], 2
    ja .bad
    mov al, [es:di+16]
    xor al, [es:di+17]
    cmp al, 0ffh
    jne .bad
    cmp word [es:di+18], 3
    jne .bad
    cmp byte [es:di+20], 1
    ja .bad
    cmp byte [es:di+21], 3
    ja .bad
    cmp byte [es:di+22], 7
    ja .bad
    cmp byte [es:di+23], 5
    ja .bad
    cmp byte [es:di+24], 18
    ja .bad
    cmp byte [es:di+63], 0a5h
    jne .bad
    mov si, di
    mov cx, 60
    call cfg_crc_es
    cmp ax, [es:di+60]
    jne .bad
    xor ax, ax
    mov si, di
    mov cx, 64
.sum:
    add al, [es:si]
    inc si
    loop .sum
    cmp al, 0c0h
    jne .bad
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.bad:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

; CRC16-CCITT over ES:SI, CX bytes; AX=result.
cfg_crc_es:
    push bx
    push cx
    push dx
    push si
    mov ax, 0ffffh
.byte:
    mov dl, [es:si]
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

; DS:SI CX bytes -> AL sum.
sum_bytes:
    xor ax, ax
.loop:
    add al, [si]
    inc si
    loop .loop
    ret
; Read-only latch of PIT channel 0. No IRQs, BIOS, or DOS are needed.
; Stock DOS timer: mode 3 decrements twice per 1.193182 MHz cycle, so a
; total 65535 count decrement takes >=27ms (mode 2 takes >=54ms).
; This exceeds AT29C257's 10ms ID/page interval. A stopped timer fails.
pause:
    pushf
    pusha
    call pit_sample
    mov bx, ax
    mov bp, 0ffffh
    xor si, si
    mov di, 128
.loop:
    call pit_sample
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
pit_sample:
    xor al, al
    out 43h, al
    in al, 40h
    mov ah, al
    in al, 40h
    xchg al, ah
    ret
flash_vector_stub:
    mov byte [cs:deferred_irq], 1
    iret
; No DOS, BIOS or external code calls. Text-mode DOS screen, mono or color.
; Do not restore IRQs or vectors: hardware reset is the final required step.
reset_screen:
    cli
    cld
    mov si, dx
    xor ax, ax
    mov es, ax
    mov ax, 0b800h
    cmp byte [es:449h], 7
    jne .video
    mov ax, 0b000h
.video:
    mov es, ax
    xor di, di
    mov ax, 0720h
    mov cx, 2000
    rep stosw
    xor di, di
    mov ah, 0fh
    xor bx, bx
.text:
    lodsb
    cmp al, '$'
    je .reset
    cmp al, 13
    je .text
    cmp al, 10
    jne .char
    add bx, 160
    mov di, bx
    jmp .text
.char:
    stosw
    jmp .text
.reset:
    mov si, reset_msg
    mov di, 160*20
.next:
    lodsb
    test al, al
    jz .stop
    stosw
    jmp .next
.stop:
    hlt
    jmp .stop
print:
    pusha
    push ds
    push es
    mov ah, 09h
    int 21h
    pop es
    pop ds
    popa
    ret
hexword:
    pusha
    mov bx, ax
    mov cx, 4
.loop:
    rol bx, 4
    mov dl, bl
    and dl, 15
    add dl, '0'
    cmp dl, '9'
    jbe .emit
    add dl, 7
.emit:
    mov ah, 02h
    int 21h
    loop .loop
    popa
    ret
%macro error 2
%1:
    mov dx, %2
    jmp fail
%endmacro
error memory_error, memory_msg
error usage_error, usage_msg
error protected_error, protected_msg
error file_error, file_msg
error image_error, image_msg
error find_error, find_msg
error backup_error, backup_msg
error clock_error, clock_msg
error id_error, id_msg
error mapping_error, mapping_msg
error cancelled, cancelled_msg
write_error:
    mov dx, write_msg
fail:
    cmp byte [window_active], 0
    jne reset_screen
    call print
    mov ax, 4c01h
    int 21h

memory_msg db 'At least 96 KiB of free conventional DOS memory is required.',13,10,'$'
intro db 'Slaton OPROM updater 0.6 - 286 DOS real mode',13,10,'$'
usage_msg db 'Usage: SLFLASH SLATON.BIN [D800] [/NOBACKUP] [/RESETCFG]',13,10
          db 'Segment is optional, 2 KiB aligned, C000..E800. Options may be reordered.',13,10,'$'
protected_msg db 'Use plain DOS without EMM386, Windows or a protected-mode monitor.',13,10,'$'
file_msg db 'Cannot read an exactly 32768-byte image.',13,10,'$'
image_msg db 'Image rejected: Slaton signature/header/checksum mismatch.',13,10,'$'
find_msg db 'Multiple ROM matches. Specify the target segment explicitly.',13,10,'$'
default_msg db 'No recognized Slaton ROM; using default card window D800.',13,10,'$'
vector_notice db 'OPROM vector (reported, not blocked): INT $'
vector_at db ' at $'
target_msg db 'Target ROM segment: $'
installed_msg db 13,10,'Installed version: $'
image_version_msg db 13,10,'Image version:     $'
unknown_msg db 'UNKNOWN','$'
retain_msg db 13,10,'Valid configuration layout 3 settings will be retained.',13,10,'$'
defaults_msg db 13,10,'No valid layout 3 settings: using factory defaults.',13,10,'$'
reset_config_msg db 13,10,'/RESETCFG selected: erasing saved settings to factory defaults.',13,10,'$'
no_backup_warning db 'WARNING: /NOBACKUP selected; no recovery file will be written.',13,10,'$'
confirm_msg db 'Disable shadowing/caching for this ROM in BIOS first.',13,10,'Keep power on. A failed update may need an external programmer.',13,10,'A numbered backup will be created before chip commands.',13,10,'Program this image? Y to continue, any other key cancels: $'
newline db 13,10,'$'
backup_name db 'SLAT0000.BAK',0
backup_index dw 0
backup_msg db 'Cannot create/write backup. No chip commands sent.',13,10,'$'
backup_ok db ' saved. Identifying AT29C257...',13,10,'$'
backup_skipped_msg db 'Backup file skipped. Identifying AT29C257...',13,10,'$'
clock_msg db 'PIT timer did not advance; operation stopped.',13,10,'$'
id_msg db 'AT29C257 ID 1F/DC not returned. Check ROM shadow/cache/write protection.',13,10,'$'
mapping_msg db 'ROM readback changed after ID. Mapping/shadowing is not suitable.',13,10,'$'
writing_msg db 'Programming and verifying from RAM; wait for completion (no progress dots).',13,10,'$'
failed_page_msg db 13,10,'FAILED at/near chip offset $'
write_msg db 'PROGRAM/VERIFY FAILED. Firmware may be incomplete.',13,10,'Retain the numbered .BAK file. External programmer recovery may be required.',13,10,'$'
success_msg db 'SUCCESS: all 32768 bytes verified.',13,10,'Programming complete.',13,10,'$'
reset_msg db 'RESET ME - press the hardware RESET button or power-cycle.',0
cancelled_msg db 13,10,'Cancelled. No writes made.',13,10,'$'
brand db 'Slaton Computers 16MB ISA Flash',0
brand_end:
deferred_irq db 0
saved_flags dw 0
window_active db 0
backup_seg dw 0
rom_seg dw 0
rom_override dw 0
known_card db 0
preserve_settings db 0
skip_backup db 0
reset_config db 0
version_result dw 0
version_prefix db 'Disk BIOS '
version_prefix_end:
version_buffer times 16 db 0
chip_id dw 0
handle dw 0
page dw 0
matches db 0
extra db 0
filename times 128 db 0
option_length db 0
option_remaining dw 0
option_buffer times 16 db 0
option_nobackup db '/NOBACKUP',0
option_nobackup_end:
option_resetcfg db '/RESETCFG',0
option_resetcfg_end:
align 2
image times 32768 db 0
stack times 512 db 0
stack_top:
%if ($-$$+100h) > 0fffeh
    %error "COM image/stack exceeds segment"
%endif
