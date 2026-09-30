# Original firmware and hardware interface

This document records what is known about the original firmware and hardware
on the 16 MB Cisco PIX ISA flash card repurposed by this project.

The card used for the hardware tests is marked **SCPOLARISFLISA1** and
**PFE120400R00**. It contains:

- one Atmel AT29C257 32 KiB EEPROM used as the PC option ROM;
- two Intel i28F640J5 64 Mbit flash devices, providing 16 MiB total;
- one Altera MAX EPM7128SQC160-15 CPLD implementing the ISA interface;
- jumpers JP1 and JP2.

The working jumper configuration is **JP1 ON, JP2 OFF**. Moving either jumper
away from that configuration prevents the option ROM from being discovered.
Cisco's installation guide likewise warns that the flash-card jumper must not
be moved, but neither the guide nor the original option ROM identifies the
individual electrical functions of JP1 and JP2.

## Evidence and confidence

The findings below use three confidence levels:

- **Confirmed in firmware** means the behavior is present in reachable code in
  the original option ROM.
- **Confirmed on hardware** means the behavior was observed on the physical
  card.
- **Unresolved** means the available evidence does not establish the
  electrical implementation inside the CPLD.

The original 32 KiB ROM analyzed during development has SHA-256:

```text
18d52c9a5bce4557bda612a6081265e18ed468432115aa6af44d2055dc8aaf1b
```

The original binary is not included in this repository. Offsets quoted below
are byte offsets within that image.

## Original option-ROM structure

The original image starts with:

```text
55 AA 1A EB 07 ...
```

`55 AA` is the PC option-ROM signature. The size byte, `1Ah`, declares 26
512-byte units, or 13,312 bytes. Those declared bytes have a valid modulo-256
checksum. The EEPROM contains 32,768 bytes in total, and the checksum of the
whole EEPROM is not zero. This is intentional: the system BIOS validates only
the declared 13,312-byte image, while the card's own loader copies and uses the
entire 32 KiB device.

The initialization entry at offset `000Ch`:

1. saves the caller's state;
2. disables interrupts while changing the interrupt vector table;
3. writes `D800:0030` into interrupt vector 19h;
4. restores the caller's state and returns to POST.

The code hard-codes segment `D800`. On the tested card the ROM occupies
physical addresses `D8000h` through `DFFFFh`, and no working relocation control
has been found.

### INT 19h boot path

The handler at ROM offset `0030h` replaces the motherboard's normal INT 19h
bootstrap path. It does not install an INT 13h disk service and does not make
the flash appear as a BIOS hard disk.

The handler:

1. copies all 32 KiB from `D800:0000` to physical address `00007C00h`;
2. loads a GDT from the relocated image;
3. enables protected mode;
4. far-jumps to 32-bit code at physical address `00007C65h`;
5. initializes direct serial output;
6. first attempts to load the PIX bootstrap from the floppy controller;
7. if the floppy path fails, discovers the data flash at I/O base `0300h` and
   loads a PIX image from it;
8. transfers control to the loaded image.

The floppy path directly programs the floppy controller and DMA hardware. That
DMA use belongs to the floppy loader, not the ISA flash card. The flash-card
path is programmed I/O and polling only.

The original ROM identifies itself as:

```text
Cisco Secure PIX Firewall BIOS (3.6)
```

Other diagnostic strings identify the two data-flash drivers compiled into the
ROM as `strata.c` and `atmel.c`. The Intel StrataFlash driver is tried first.
The Atmel data-flash driver supports older card hardware and is unrelated to
the AT29C257 option-ROM EEPROM.

### Stored PIX image

After identifying the Intel flash, the original bootloader reads a 32-byte
header at flash offset zero. It requires the first 32-bit field to equal
`00000107h`. Header fields are then used to calculate and round the stored image
length, after which the loader reads the image into RAM and enters it through
its executable-image loader.

The bootloader therefore understands a Cisco image container. It does not
understand an MBR, partition table, FAT filesystem, or BIOS disk sectors. This
is the principal architectural difference between the original firmware and
the replacement disk BIOS in this repository.

## Data-flash register interface

The i28F640J5 path uses six word-wide I/O registers beginning at `0300h`.
Every access in the original Intel driver is `IN AX,DX` or `OUT DX,AX`.

| Port | Direction | Confirmed purpose |
|---|---|---|
| `0300h` | Write | 128 KiB erase-block/page number within the selected chip |
| `0302h` | Write | Word address within the selected block |
| `0304h` | Write | Low address/selector value; normally zero for aligned word transfers |
| `0306h` | Read/write | Intel flash data, command, identifier, CFI, and status word |
| `0308h` | Write | CPLD control strobes and flash-bank selection |
| `030Ah` | Read | CPLD ready status; bit `0020h` indicates ready |

No independent card register has been observed at an odd I/O address. If an
ISA host splits one of these word cycles, the following odd address carries the
upper byte; no firmware treats that odd address as a separate register.

### Address calculation

The complete address calculation within an 8 MiB chip is:

```text
port 0300h = (address & 007E0000h) >> 17
port 0302h = (address & 0001FFFEh) >> 1
port 0304h = address & 1
```

For normal word-aligned transfers, port `0304h` receives zero. The word address
at `0302h` runs from `0000h` through `FFFFh`; the value at `0300h` selects one
of 64 128 KiB erase blocks.

The older BIOS 3.6 loader masks the block field with `003E0000h`, so that
particular loader addresses only the lower 4 MiB. Its boot image is much
smaller than that. Using `007E0000h` addresses all 8 MiB of a selected
i28F640J5.

Hardware testing showed that values zero and one at `0304h` returned the same
identifier, CFI information, and sampled array contents. Port `0304h` is not
the second-chip selector.

### Read cycle

The original option ROM's read primitive performs this sequence, with bank
bits added for access to both chips:

```text
OUT 0300h, block
OUT 0302h, word_address
OUT 0304h, selector
OUT 0308h, bank_bits | 0008h
IN  AX,0306h
```

Sequential reads update `0302h` for every word. No memory-mapped data window is
used by the original direct ISA driver.

### Command and write cycle

Intel commands and data words are issued through `0306h`. The CPLD is then
strobed through `0308h`:

```text
OUT 0300h, block
OUT 0302h, word_address
OUT 0304h, selector
OUT 0306h, command_or_data
OUT 0308h, bank_bits | 0014h
OUT 0308h, bank_bits | 0010h
```

The Intel command set supported by the device includes:

| Command | Value | Use |
|---|---:|---|
| Read array | `00FFh` | Return the selected chip to normal reads |
| Read identifier | `0090h` | Read manufacturer and device identifiers |
| CFI query | `0098h` | Read the Common Flash Interface table |
| Clear status | `0050h` | Clear prior program/erase errors |
| Erase setup | `0020h` | Begin an erase command sequence |
| Erase confirm | `00D0h` | Confirm block erase |
| Buffered program | `00E8h` | Request the flash write buffer |
| Buffered confirm | `00D0h` | Commit a filled write buffer |

The firmware waits for CPLD-ready bit `0020h` at port `030Ah`, then reads the
Intel status register through `0306h` and checks status bit 7 plus the Intel
error bits. Erase and programming are therefore synchronous from the caller's
perspective.

The i28F640J5 CFI table reports a 32-byte write buffer. A buffered program
operation can consequently write at most 16 words per command sequence.

## Flash identification and geometry

The physical card and the original option ROM's identifier checks agree on:

```text
Manufacturer: 0089h (Intel)
Device:       0015h (i28F640J5)
```

The hardware CFI response contains `QRY` at word offsets `10h` through `12h`
and reports:

- Intel/Sharp command set `0001h`;
- device size exponent `17h`, or 2^23 bytes = 8 MiB per chip;
- x8/x16 device interface support;
- a 32-byte maximum write buffer;
- one erase region;
- 64 erase blocks per chip;
- 128 KiB per erase block.

With two chips, the physical geometry is therefore:

```text
2 chips x 8 MiB                         = 16 MiB
64 blocks/chip x 2 chips               = 128 blocks
128 KiB/block x 128 blocks             = 16 MiB
```

## Second 8 MiB bank

The address registers at `0300h`, `0302h`, and `0304h` do not select the second
chip. Initial experiments with additional address bits aliased back to the
first chip.

Control bit `0020h` is the bank mask. A destructive physical test confirmed it:

- control bit clear selected one i28F640J5;
- control bit `0020h` selected the other i28F640J5;
- independent markers remained distinct between the two banks;
- erase/program/readback marker pairs passed in all 128 erase blocks;
- the total independently retained capacity was 16 MiB.

The complete control values used by the replacement firmware are consequently:

| Operation | Bank 0 | Bank 1 |
|---|---:|---:|
| Read enable | `0008h` | `0028h` |
| Command strobe high | `0014h` | `0034h` |
| Command strobe low | `0010h` | `0030h` |

The 128-block test wrote a small marker in every erase block. It established
bank independence and address coverage; it was not an every-byte RAM-style
test.

## Option-ROM EEPROM and updates

The AT29C257 is mapped at `D8000h` through `DFFFFh` on the tested card. Its
software-ID mode returns manufacturer/device bytes `1Fh/DCh`, and its
programming interface accepts 64-byte pages through the memory-mapped ROM
address.

The option ROM can be updated in-system. It does not use the data-flash I/O
registers for that operation. ROM shadowing, caching, or motherboard write protection can
redirect or suppress writes, which is why `SLFLASH` requires those features to
be disabled and verifies the complete 32 KiB after programming.

## Polling, interrupts, and DMA

The original option ROM's Intel flash path uses polling. Its routines:

- poll ready bit `0020h` at `030Ah`;
- poll the i28F640J5 status register through `0306h`;
- do not assign an ISA IRQ to the flash card;
- do not install a flash-card interrupt handler;
- do not unmask or acknowledge a PIC line for the card;
- do not configure an ISA DMA channel for flash transfers.

The original bootloader does configure DMA for its floppy path. That code is
separate from the flash driver. Likewise, INT 19h and the replacement
firmware's INT 13h hook are software interrupt vectors, not card-generated ISA
interrupt requests.

The CPLD may have unused connections or dormant capabilities, but the available
evidence does not establish an interrupt mode. The supported model
is polling-only programmed I/O.

## 16-bit ISA requirement

The i28F640J5 driver deliberately uses word I/O for every address, control,
status, command, and data transfer. Although the original ROM contains generic
byte-I/O helpers for other hardware, the J5 driver never uses them and contains
no 8-bit fallback or bus-width detection.

The physical card was also tested with the entire 16-bit ISA extension section
electrically masked. In that configuration the motherboard did not discover
the option ROM at all. The failure occurred before any firmware could execute.

This confirms the practical requirement:

```text
286 or newer x86 PC with a 16-bit ISA slot
```

An option-ROM menu setting or alternate byte-I/O routine cannot compensate for
the missing extension signals because the ROM itself is not decoded without
them. The exact extension signal used by the CPLD's ROM decode remains
unresolved.

## Memory-mapped access

The original direct card driver at I/O base `0300h` is entirely port based.
No direct memory-mapped data-flash aperture was found in the option ROM.

Motherboard PCI configuration registers can provide a temporary flash frame on
certain PIX platforms. That mechanism depends on the motherboard chipset
and is not evidence that this standalone ISA card exposes a conventional ISA
memory window. No such window has been confirmed on the tested PC.

The only confirmed memory-mapped component on the card is the AT29C257 option
ROM at `D8000h`.

## Jumper observations

The tested combinations produced:

| JP1 | JP2 | Result |
|---|---|---|
| ON | OFF | Option ROM and data flash operate |
| Any other tested combination | Any other tested combination | Option ROM not discovered |

The jumpers therefore affect card-level enable or decode behavior rather than
providing a usable second-bank selection. Bank selection is performed through
bit `0020h` at port `0308h`.

The precise electrical role of each jumper is unresolved without PCB tracing
or the EPM7128 logic equations.

## Register summary

For software using both chips, an aligned physical byte offset can be reduced
to the following values:

```text
bank       = (offset >= 00800000h) ? 0020h : 0000h
within     = offset & 007FFFFFh
block      = (within >> 17) & 003Fh
word       = (within >> 1)  & FFFFh
selector   = within & 1
```

The common primitives are:

```text
select(offset):
    OUT 0300h, block
    OUT 0302h, word
    OUT 0304h, selector

read_word(offset):
    select(offset)
    OUT 0308h, bank | 0008h
    IN  AX,0306h

write_command(offset, value):
    select(offset)
    OUT 0306h, value
    OUT 0308h, bank | 0014h
    OUT 0308h, bank | 0010h

board_ready():
    return (IN 030Ah & 0020h) != 0
```

These operations are the confirmed hardware foundation used by the replacement
BIOS and DOS diagnostic utilities.

## Remaining unknowns

The following have not been established:

- the EPM7128 internal logic equations;
- the exact CPLD pins and ISA extension signal that gate option-ROM decode;
- the electrical purpose of JP1 and JP2 individually;
- whether unused CPLD logic can generate an IRQ;
- whether a motherboard-specific memory frame can be created outside original
  PIX hardware;
- endurance or bad-block behavior of individual used cards.

These unknowns do not affect the confirmed 16 MiB, word-I/O, polling interface
used by this project.

## Primary references

- Intel, *5 Volt Intel StrataFlash Memory: 28F320J5 and 28F640J5*, order
  290606: <https://docs.rs-online.com/6518/0900766b800ac4d7.pdf>
- Microchip/Atmel, *AT29C257 256K 5-volt Only CMOS Flash Memory*:
  <https://www.mouser.com/datasheet/2/268/Atmel_AT29C257-1180384.pdf>
- Cisco, *PIX Firewall 16 MB Flash Circuit Board* installation section:
  <https://www.cisco.com/en/US/docs/security/pix/pix53/hw/installation/guide/board.html>
