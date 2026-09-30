# PIX DOS disk format v1 — 512 KiB reservation

The disk uses both independently addressable 8 MiB flash banks.

Physical flash: 128 erase blocks of 131072 bytes, two independently selected
8 MiB banks. Each block contains a 32-byte header, 252 eight-byte record
entries (2048 metadata bytes total), then 252 512-byte data slots.
DOS capacity: 31744 sectors, 16252928 bytes = 15.5 MiB.
CHS: 496 cylinders, four heads, 16 sectors per track.

The 524288-byte reservation consists of 262144 bytes of per-block metadata,
258048 bytes of data slots in two spare blocks, and eight additional data
slots (4096 bytes) of slack. Both chips participate in allocation.

## Record commit

Each metadata entry is LBA16, complement-of-LBA16, payload CRC16-CCITT,
commit16. The entry is initially all FFFF. Program and verify its first
six bytes BEFORE touching its data slot. Program and verify the 512-byte
payload, then program and verify commit=0000 last. Every nonblank entry
consumes its slot, including an interrupted write. Thus a partially programmed
data slot can never be mistaken for an erased reusable slot after restart.

The newest committed record wins: first by 32-bit block generation, then by
slot number. A 16-bit RAM map identifies each logical sector's current
physical block/slot. Unwritten logical sectors read as zeros. Data CRC is
checked on reads, including reads used during reclamation.

## Block header

Offsets: magic/version[8] at 0, generation32 at 8, complement32 at 12,
GC source generation32 at 16, GC source block16 at 20 (FFFF for normal
allocation), CRC16 over bytes 0..21 at 22, CRC complement16 at 24,
logical-sector count16 at 26, mutable GC state16 at 28,
header commit16 at 30. Header commit is programmed last.
GC state FFFF means pending; FFFE means complete. It is excluded from CRC.
Generation zero and generation wrap are forbidden.

## Reclamation and interrupted reclamation

Allocate one spare as a replacement with a PENDING header naming the source
block and source generation. Copy only records that the current map still
points to in that source. Commit each copy normally. After every live record
is copied, commit GC state=FFFE. Only THEN erase the source. New user writes
may enter the replacement only after reclamation completes.

On startup, discard a PENDING replacement only after confirming its named
source header/generation remains intact. It contains no new user writes,
and its source was never erased. This avoids accumulated torn-copy slots
exhausting replacement capacity across repeated power failures.

A COMPLETE replacement authorizes retiring its named source only if that
source still has the recorded generation. Reuse of the same physical block
with a newer generation is not mistaken for the old source. Retired sources
are excluded before reconstructing mappings, including when their erase
was interrupted. Damaged/inconsistent recovery evidence stops mounting.

Writes are serialized. A hardware error makes the mounted instance fail
further disk operations until reboot/recovery; it must not continue into a
partly completed reclamation transaction.

## BIOS and RAM

286 real mode, CHS INT 13h services, 512-byte transfers. The initial firmware
uses an 80 KiB conventional-memory reservation: 16 KiB resident code/state/
private stack plus 64 KiB for the 63488-byte sector map. No 128 KiB buffer.
Setup selects disk 80h (C: target) or 81h (D: target, requiring an existing
first disk). Native disks at or above the insertion point are shifted upward
through an INT 13h wrapper. DOS assigns letters from its recognized partitions. Standard DOS boot media can install the
MBR, partition, FAT filesystem, and boot files through ordinary BIOS calls.

## Limits

This protects the prescribed write/erase ordering against interrupted
operations under normal NOR behavior. It does not make arbitrary flash-chip
failure, worn-out cells, controller faults, or CRC collisions recoverable.
No bad-block reserve beyond the specified budget is promised. Reclamation
can cause long write pauses near full capacity. Acknowledged writes must
be committed to flash before INT 13h reports success; no volatile write-back.

## OPROM configuration

The 32768-byte chip image declares a 16384-byte BIOS option-ROM image; its
checksum byte is at 3FFF. Two 64-byte settings pages at 7F80 and 7FC0 lie
outside that range. Each holds magic SLATCFG1, a generation and its inverse,
a BIOS disk-mode choice and its inverse, format version, CRC16, a marker, and a balancing
byte. Each complete settings page has the same modulo-256 sum as an erased
64-byte page (C0). Both the declared firmware checksum and the factory full
chip checksum are zero; valid settings saves preserve both. A torn settings
write cannot affect the declared firmware checksum.

Settings use the AT29C257 software-ID and protected 64-byte page-programming
sequences described in the manufacturer
[datasheet](https://www.mouser.com/datasheet/2/268/Atmel_AT29C257-1180384.pdf).
Programming occurs from RAM, with interrupts disabled while loading all 64
bytes. The inactive copy is written; the active copy remains untouched.
Readback must match before reporting a saved choice. OPROM programming on
the actual board remains unverified. ROM shadowing/caching or write protection
can prevent it; the menu reports NOT SAVED and permits a boot-only selection.
The ID command addresses include 2AAA; on writable ROM-shadow RAM these
commands can alter the shadow copy, although the already-relocated firmware
continues to run. Disable shadowing for the card ROM before saving settings.
