# Setup and optional serial diagnostics

## Install

Use the complete 32768-byte SLATON.BIN, or unpack the DOS package and
run `SLFLASH SLATON.BIN`. The updater preserves fully valid current-layout
settings unless `/RESETCFG` is selected, then ends with RESET ME. JP1 stays
ON and JP2 OFF.

## Serial is OFF by default

Factory settings disable serial access. The current configuration layout
retains all menu choices across boots after Save Settings succeeds.

When OFF, the option ROM performs no UART identification, register reads,
register writes, input polling, or output. The wrapper has no UART code.
Configuration is loaded in RAM before the first conditional serial operation.
SLSTAT also no longer reads UART ports. BIOS disk diagnostics return before
formatting strings/dumps when serial is disabled. RAM request history remains.

When ON, the selected UART is dedicated to the card's diagnostics. Choices:

- Port: COM1 03F8h, COM2 02F8h, COM3 03E8h, COM4 02E8h.
- Baud: 115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200.
- Format: 8N1, 7E1, 7O1, 8E1, 8O1, 8N2.
- Flow control: none.

Changing settings applies them for the current boot. Turning debugging OFF
immediately stops UART access; it does not touch the old UART to clean it up.
Saving persists the selection; it does not leave setup. Early wrapper serial
messages are intentionally absent. Enabled diagnostics begin only after the
saved configuration has been loaded.

## Menu

The configurable prompt displays a changing countdown. S opens setup; Enter
or Esc continues boot. In setup:

- Up/Down or Tab selects a highlighted row.
- Left/Right changes a setting. Enter or Space also cycles setting rows.
- Enter activates action rows.
- Save Settings writes the OPROM and stays in setup.
- Continue Boot uses the current choices without implicitly saving.
- Discard Changes and Continue restores the last saved choices.
- Restore Defaults selects BIOS 80h, serial OFF, COM1, 115200, 8N1, and a
  five-second delay; save explicitly to persist those defaults.
- Erase Flash Disk opens the existing Enter=quick / F=full confirmation.
- Esc continues without saving. S is ignored inside the menu, including
  buffered or typematic repeats from pressing S to enter it.

The local display uses a highlighted selection and a status line. Serial
input accepts ASCII/ANSI arrow keys while enabled; the graphical menu itself
is rendered on the local display, not replicated as a terminal screen.

## Implementation and validation

Runtime code/data stay below 16 KiB. Setup-only code is copied into the first
part of the future sector map, runs before mount, then is discarded when the
map is populated. The reservation remains 80 KiB. The ROM loader stores its
saved return stack AFTER copying the payload, protecting it from padding.
No setup-overlay code is called after mount.

The declared BIOS checksum remains the first 16 KiB. Executable setup data
also occupies part of the second half of the chip. Offset 7F7F balances the
full-chip checksum; redundant settings remain at 7F80 and 7FC0. Both factory
checksum sums are zero. Config saves retain those checksum properties.

Configuration version 3 stores four serial fields at offsets 20..23 and the
boot-delay index at offset 24. Other layout versions are rejected and the
factory defaults are used. Torn-page recovery, CRC validation, and disk-mode
complement checks remain unchanged.

Build checks cover 286 assembly, ROM size, signature, both checksums, and
blank factory settings pages. The build uses the shell scripts in the
repository root. Disk format and existing disk fixes are unchanged.

## Sector-independent operation

The BIOS mounts its internal flash mapping without inspecting logical sectors.
It does not require or interpret an MBR, partition table, filesystem type, or
boot-sector signature. A normal BIOS read/write request treats every logical
sector identically, including sector zero. Motherboard firmware and the booted
operating system decide how to interpret the disk's contents.

## Disk mode and boot delay

The disk-mode setting uses consistent BIOS-number wording:

- `C: target (BIOS drive 80h)`
- `D: target (BIOS drive 81h)`
- `None (disk disabled)`

None leaves the configured setup prompt available on every boot,
using the configured delay, but does not mount the data flash, hook INT 13h,
or change the BIOS hard-disk count after the prompt closes.

Boot delay is adjustable from 2 through 20 seconds. It controls the visible
setup countdown. The factory default is 5 seconds. Configuration version 3
stores the selected delay with the existing CRC and redundant-page recovery
fields.
