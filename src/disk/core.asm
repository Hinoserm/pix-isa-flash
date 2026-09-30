CPU 286
BITS 16
ORG 0
%define LOGICAL_SECTORS 31744
%define SLOTS 252
%define CORE_STACK (16384-64)
    jmp near initialize
; Fixed diagnostic ABI at CS:0003, readable without UART or disk calls.
    db 'SCDBG014'
dbg_calls dd 0                  ; +11: hard-disk INT13 entries (0.8+)
dbg_last_ax dw 0                ; +15
dbg_last_cx dw 0                ; +17
dbg_last_dx dw 0                ; +19
dbg_phase dw 0                  ; +21: 0 entry,1 ours,2 floppy,3 native,4 done
dbg_uart_timeouts dw 0          ; +23: wraps, never disables logging
dbg_uart_skips dw 0             ; +25: incompatible LCR or active IRQ driver
dbg_map dw 0                   ; +27: history segment
history_head dw 0              ; +29: next slot, 0..31
history_current dw 0
%include "disk/console.inc"
%include "disk/hw.inc"
%include "disk/ftl.inc"
%include "disk/bios.inc"
%include "disk/debug.inc"

initialize:
    cld
    push cs
    pop ds
    mov [oprom_segment], bx
    push cs
    pop es
    call setup_load
    call serial_apply
    mov si, dbg_init
    call trace_puts
    call trace_platform
    call clear_screen
    mov si, banner
    call puts
    mov ax, cs
    add ax, 400h
    mov [map_segment], ax
    mov [dbg_map], ax
    mov es, ax
    mov di, 0f800h
    xor ax, ax
    mov cx, 1024
    rep stosw
    mov word [cur_block], 0
    mov word [cur_word], 0
    call board_reset
    jc .failed
    call identify
    jc .missing
    mov word [cur_block], 64
    call identify
    jc .missing
    mov word [cur_block], 0
    xor ax, ax
    mov es, ax
    mov al, [es:475h]
    cmp al, 126
    ja .failed
    mov [old_drive_count], al
    inc al
    mov [drive_count], al
    mov si, dbg_cfg
    call trace_puts
    xor ax, ax
    mov al, [saved_drive]
    call trace_hex
    call trace_nl
    call setup_prompt
    cmp byte [fatal_error], 0
    jne .failed
    cmp byte [erase_reboot], 0
    jne .erased
    cmp byte [chosen_drive], 2
    je .disabled
    call apply_drive_choice
    mov si, mount_msg
    call puts
    mov si, dbg_mount
    call trace_puts
    call ftl_mount
    jc .failed
    mov si, dbg_mounted
    call trace_puts
    mov ax, [free_blocks]
    call trace_hex
    call trace_nl
    cmp word [active_block], 0ffffh
    jne .install
    mov si, new_msg
    call puts
    call ensure_space
    jc .failed
.install:
    xor ax, ax
    mov es, ax
    mov al, [drive_count]
    cli
    mov [es:475h], al
    mov ax, [es:4ch]
    mov [old13], ax
    mov ax, [es:4eh]
    mov [old13+2], ax
    mov word [es:4ch], int13_handler
    mov [es:4eh], cs
    cmp byte [virtual_drive], 80h
    jne .installed
    ; Preserve the native first disk's parameter vector when it exists,
    ; since the previous BIOS still uses it for translated native requests.
    cmp byte [old_drive_count], 0
    jne .installed
    mov word [es:104h], disk_parameter_table
    mov [es:106h], cs
.installed:
    sti
    mov si, dbg_installed
    call trace_puts
    call trace_platform
    mov si, installed_c_msg
    cmp byte [virtual_drive], 80h
    je .announce
    mov si, installed_d_msg
.announce:
    call puts
    mov si, ready_msg
    call puts
    mov ax, 1
    retf
.erased:
    xor ax, ax
    retf
.disabled:
    mov si, disabled_msg
    call puts
    xor ax, ax
    retf
.missing:
    mov si, missing_msg
    call puts
    xor ax, ax
    retf

.failed:
    mov si, dbg_failure
    call trace_puts
    mov ax, [flash_status]
    call trace_hex
    call trace_nl
    mov si, failed_msg
    call puts
    mov ax, [flash_status]
    call hexword
    call newline
    xor ax, ax
    retf

align 2
header_template:
    db 'PXDF0001'
    dd 0,0,0
    dw 0ffffh,0,0,LOGICAL_SECTORS,0ffffh,0ffffh
commit_zero dw 0
gc_complete_word dw 0fffeh
disk_parameter_table:
    dw 496
    db 4
    dw 0,0ffffh
    db 0,0,0,0,0
    dw 495
    db 16,0
oprom_segment dw 0
old13 dd 0
map_segment dw 0
cur_block dw 0
cur_word dw 0
cur_bank dw 0
flash_status dw 0fffeh
error_mask dw 0
watch_low dw 0
watch_high dw 0
prog_src dw 0
prog_count dw 0
prog_start dw 0
fatal_error db 0
io_busy db 0
last_status db 0
request_status db 0
request_fn db 0
io_done db 0
old_drive_count db 0
drive_count db 0
virtual_drive db 80h
align 2
entry_ss dw 0
entry_bp dw 0
request_ax dw 0
r_ax dw 0
r_bx dw 0
r_cx dw 0
r_dx dw 0
r_es dw 0
r_si dw 0
r_di dw 0
io_count dw 0
io_lba dw 0
buffer_seg dw 0
buffer_off dw 0
free_blocks dw 0
active_block dw 0ffffh
alloc_cursor dw 0
max_gen_lo dw 0
max_gen_hi dw 0
mount_block dw 0
mount_slot dw 0
create_id dw 0
create_source dw 0
read_lba dw 0
read_phys dw 0
record_lba dw 0
record_slot dw 0
gc_source dw 0
gc_slot dw 0
block_state times 128 db 0
block_gen_lo times 128 dw 0
block_gen_hi times 128 dw 0
block_src_lo times 128 dw 0
block_src_hi times 128 dw 0
block_source times 128 dw 0
block_gc_state times 128 dw 0
block_tail times 128 dw 0
block_live times 128 dw 0
block_tables_end:
header_buf times 32 db 0
record_buf times 8 db 0
verify_buf times 32 db 0
sector_buf times 512 db 0
meta_buf times 2016 db 0
banner db 'Slaton Computers 16MB ISA Flash',13,10
       db 'Disk BIOS 0.14 - 286 - configurable serial diagnostics',13,10,0
mount_msg db 'Mounting flash disk...',13,10,0
new_msg db 'No disk metadata: initializing an empty 15.5 MiB disk.',13,10,0
installed_c_msg db 'Installed as first BIOS hard disk (80h).',13,10,0
installed_d_msg db 'Installed as second BIOS hard disk (81h).',13,10,0
ready_msg db '15.5 MiB, 496/4/16 CHS; 512 KiB flash reserve.',13,10
          db '80 KiB resident RAM.',13,10,0
missing_msg db 'Slaton disk not installed: Intel flash ID missing in one or both banks.',13,10,0
failed_msg db 'Slaton disk not installed: flash/recovery failure, status ',0
disabled_msg db 'Slaton flash disk disabled in setup; INT 13h not installed.',13,10,0
%if ($-$$) > CORE_STACK-2048
    %error "Disk core leaves less than 2 KiB of private stack"
%endif

resident_end:
; Setup executes here before ftl_mount overwrites this area with the map.
    times 16384-($-$$) db 0ffh
setup_overlay:
%include "disk/setup.inc"
setup_overlay_end:
