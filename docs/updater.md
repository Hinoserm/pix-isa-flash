# DOS OPROM updater

`SLFLASH SLATON.BIN` displays the installed and incoming firmware versions
before confirmation. It searches for the bounded version string only in a
recognized Slaton ROM. Unrecognized or blank contents display UNKNOWN and
remain eligible for programming; no existing firmware code is executed.

A recognized Slaton ROM is located automatically. If none is found, the
updater uses segment D800. Override the target with a four-digit, 2 KiB-aligned
segment between C000 and E800:

```
SLFLASH SLATON.BIN D800
```

Multiple recognized ROM windows require an explicit segment. Confirm the
printed segment before programming. Version recognition is independent of
hardware identification: the chip must return AT29C257 ID 1F/DC before any
page programming. The input must still be a valid 32 KiB Slaton ROM image.

Options may follow the filename and optional segment in any order:

```
SLFLASH SLATON.BIN /NOBACKUP
SLFLASH SLATON.BIN /RESETCFG
SLFLASH SLATON.BIN D800 /RESETCFG /NOBACKUP
```

`/NOBACKUP` suppresses creation of the numbered recovery file. The updater
still snapshots the installed ROM in RAM for identification and verification,
and prints a warning before confirmation. `/RESETCFG` does not carry forward
installed settings; the incoming image's factory defaults are programmed.

`SLFLASH /?` prints usage and returns success. Invoking `SLFLASH` without an
image prints the same usage and returns an error. These paths run before the
protected-mode and 96 KiB memory checks.

The original 32 KiB are backed up to the next unused SLATxxxx.BAK file.
Settings pages are preserved only when at least one page passes the complete
current layout-3 checks: magic, generation complements, field ranges, CRC,
marker, and page checksum. Unknown, blank, invalid, reset-requested, or other
layouts use the new image's factory defaults. Programming executes from DOS RAM without
DOS/BIOS calls or maskable interrupts; final status is written directly to
text video memory. After SUCCESS / RESET ME, use hardware reset or power-cycle.
