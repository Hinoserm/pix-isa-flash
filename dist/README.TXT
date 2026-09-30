SLATON DISK BIOS 0.14 / DOS OPROM UPDATER 0.6

Run SLFLASH SLATON.BIN from plain DOS, with ROM shadowing/caching disabled.
The updater makes a numbered backup, preserves saved settings, verifies the
full 32 KiB, displays RESET ME and stops. Use hardware reset after SUCCESS.
JP1 ON / JP2 OFF. The updater needs 96 KiB free RAM and 32 KiB backup space.

SERIAL DEBUGGING IS OFF BY DEFAULT.
OFF means NO UART detection, reads, writes, input polling, or output. Trace
formatting/dumps are skipped as well. RAM request history remains available.

S opens setup during the visible, configurable countdown.
Arrow keys select/change items. Enter activates the selected row.
SAVE SETTINGS saves and STAYS IN SETUP. CONTINUE BOOT is a separate action.
DISCARD CHANGES AND CONTINUE restores saved choices. Esc continues unsaved.
S has no action inside setup, so repeated S presses cannot save or exit.

Serial settings: OFF/ON, COM1-COM4, 115200 through 1200 baud,
8N1 / 7E1 / 7O1 / 8E1 / 8O1 / 8N2. No flow control.
When enabled, the selected UART is dedicated to firmware diagnostics.
Settings apply immediately for this boot; use Save Settings to persist them.

ERASE FLASH DISK: Enter = quick metadata invalidation; F = full chip erase.
Other key cancels. Reboot afterward to initialize an empty disk.
OPROM settings are retained. Quick erase does not securely wipe payloads.

SLSTAT.COM shows read-only flash metadata usage. /D adds RAM diagnostics.
It does not access any UART. Plain real-mode DOS and 64 KiB RAM required.
No floppy diagnostic code is run. The system BIOS retains floppy booting.

See flash-statistics.md for settings, layout and validation details. The storage
format and 80 KiB RAM reservation are unchanged. No CPU emulator was used.
This revision has been assembled and statically audited, not hardware tested.

The BIOS treats all disk sectors as opaque data. It does not read or interpret
an MBR, partition table, filesystem, or boot-sector signature at startup.
The internal flash mapping format is independent of disk-sector contents.

SLFLASH 0.6 shows installed and image versions. Unknown/blank installed ROMs
show UNKNOWN and can be programmed. Default unknown-card segment is D800;
use SLFLASH SLATON.BIN D800 to specify a mapped segment explicitly.
Hardware ID 1F/DC is still required. Valid layout-3 settings are retained;
other layouts receive factory defaults. Keep the backup.

/NOBACKUP skips the numbered backup file and displays a warning before
confirmation. /RESETCFG erases saved setup choices to factory defaults.
Options and the optional D800 segment may be supplied in any order.
SLFLASH /? prints usage and exits successfully. SLFLASH with no image prints
the same usage and exits with an error.

Disk mode choices are C: target (BIOS drive 80h), D: target (BIOS drive 81h),
and None (disk disabled). None keeps the setup countdown but does not mount
the disk, hook INT 13h, or alter the BIOS hard-disk count. Boot delay is adjustable
from 2 through 20 seconds and defaults to 5. Only the current settings layout
is accepted; other versions use all factory defaults.
