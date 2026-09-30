# pix-isa-flash

Replacement x86 option-ROM firmware that turns a Cisco PIX ISA flash card
into a BIOS hard disk for DOS. The card identifies as **Slaton Computers
16MB ISA Flash**.

Supports 286 and newer PCs and provides a **15.5 MiB disk** through standard
INT 13h CHS calls. The remaining 512 KiB holds metadata and spare blocks.
It reserves 80 KiB of conventional RAM. The motherboard BIOS retains control
of floppy booting and boot order.

## Hardware

These cards are often marked **SCPOLARISFLISA1** and **PFE120400R00**.

The supported card has an AT29C257 32 KiB option ROM, two Intel i28F640J5
flash chips, and an Altera EPM7128SQC160-15 CPLD. Data-flash I/O starts at
0300h. The working jumper setting is **JP1 ON, JP2 OFF**.

On the tested card, the 32 KiB option ROM is mapped at **D8000h-DFFFFh**
(segment **D800**). This mapping appears fixed; no working relocation setting
has been identified.

## Install or update

Current firmware: **0.14**. Download and unpack
[the DOS update package](dist/SLFLASH-DOS-0.14.zip).
The ZIP contains only `SLATON.BIN`, `SLFLASH.COM`, `SLSTAT.COM`, and a single
plain-text `README.TXT` with the operating instructions.

The [SLATON.BIN](dist/SLATON.BIN) image is a complete padded and checksummed
32,768-byte image. It can be written with an external programmer or with
the DOS updater when the chip is mapped and writable in the machine.

Boot plain DOS without EMM386 or Windows, disable shadowing/caching for the
card ROM, and run:

```dos
SLFLASH SLATON.BIN
```

The updater shows the installed and incoming firmware versions. Unrecognized
or blank firmware displays **UNKNOWN** and does not prevent proceeding. A
recognized Slaton ROM is located automatically; otherwise the default target
is segment D800. An optional segment overrides detection:

```dos
SLFLASH SLATON.BIN D800
```

Check the displayed target segment before confirming. The updater still
requires AT29C257 hardware identification before page programming. It creates
a numbered backup and preserves settings only when a fully valid current
layout-3 settings page exists; otherwise it uses factory defaults. After full verification and
**SUCCESS / RESET ME**, use hardware reset or power-cycle. An interrupted or
failed flash can require an external programmer.

Optional updater switches may be used in any order:

```dos
SLFLASH SLATON.BIN /NOBACKUP
SLFLASH SLATON.BIN /RESETCFG
SLFLASH SLATON.BIN D800 /NOBACKUP /RESETCFG
```

`/NOBACKUP` skips creation of the recovery file; the updater still keeps an
in-memory snapshot for validation. `/RESETCFG` erases saved menu settings and
installs the factory defaults. The updater prints a warning before confirming
when backup creation is disabled.

`SLFLASH /?` prints the complete command syntax and exits successfully.
Running `SLFLASH` without an image prints the same syntax and returns an error.

Boot a DOS installation floppy, partition the card with FDISK, reboot, and
format/install DOS normally.

The disk BIOS does not inspect partitions, filesystems, or boot-sector
signatures. It installs after mounting its internal flash mapping and serves
opaque 512-byte sectors; boot policy belongs to the motherboard BIOS.

## Setup and diagnostics

Press **S** during the configured countdown. Use the arrow keys and Enter.
Save Settings stays in the menu; Continue Boot is separate.

- Select `C: target (BIOS drive 80h)`, `D: target (BIOS drive 81h)`, or
  `None (disk disabled)`. An existing 80h disk is not required to select 81h;
  DOS determines drive letters. None still shows the setup countdown, then
  skips flash mount, INT 13h installation, and BIOS hard-disk-count changes.
- Set the boot/setup countdown from 2 through 20 seconds. The factory default
  is 5 seconds.
- Serial debugging defaults to **OFF**. When off, the ROM does not access
  any UART. When enabled, select COM1-COM4, baud rate, and line format.
- Erase Flash Disk: **Enter** performs quick metadata invalidation; **F**
  erases both chips. Both destroy the logical disk; reboot to reinitialize.
- Flash Usage and Metadata shows allocation, reclaimable blocks, and record
  statistics. `SLSTAT` gives the same read-only report under DOS; `SLSTAT /D`
  adds request-history diagnostics. Allocation counts are not DOS free space.

See [setup details](docs/setup.md), [flash statistics](docs/flash-statistics.md),
[the storage format](docs/disk-design.md), and the
[original-ROM and hardware reverse engineering notes](docs/original-rom-and-hardware.md).

## Build

Requires a POSIX shell, NASM, and standard Unix tools (`awk`, `od`, `dd`, `wc`).

```sh
sh build.sh
```

This builds only the current firmware and utilities:

- `bin/SLATON.BIN`
- `bin/SLFLASH.COM`
- `bin/SLSTAT.COM`

The build checks the ROM size, signature, checksums, and factory settings
pages. To check an image separately:

```sh
sh check.sh bin/SLATON.BIN
```

To refresh the prebuilt files and ZIP in `dist/` (requires `zip` and
`sha256sum`):

```sh
sh package.sh
```

`src/disk/` contains the BIOS; `src/tools/` contains the DOS utilities and
shared metadata scanner. Minimal DOS boot from the card has been demonstrated
on hardware. Static checks do not establish compatibility with every BIOS
or DOS version.

## License

[MIT](LICENSE).
