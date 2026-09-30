# Flash statistics

## Erase display

Quick and full erase now use a centered status window, a 40-cell progress
bar, completed-block count (0..128), and integer percentage (0..100%).
The display advances after successful completion of each block operation.
The selected mode, completion, and failure states are explicit.
Controls remain Enter=quick metadata invalidation, F=full erase, other=cancel.
Reinitialization still happens on the next boot; OPROM settings are retained.

## DOS usage

Run `SLSTAT` for the flash allocation report. It can run without upgrading
the ROM if the card already uses the existing Slaton disk format and I/O
0300h. Use plain real-mode DOS with at least 64 KiB available to the program.
`SLSTAT /D` adds the previous resident-counter/register/history diagnostics.
No UART ports are read or written by this utility.

## Setup usage

Press S, select **Flash usage and metadata...**, and press Enter.
R refreshes; Enter or Esc returns to setup. This is available before mount,
including when the metadata would prevent normal disk installation.

## Report definitions

| Field | Meaning |
|---|---|
| Allocated blocks | Valid, completed generations not retired by an exact-generation GC replacement |
| Blank headers | Unallocated blocks whose 32-byte header reads all FF |
| Invalid / quick-erased headers | Nonblank headers that fail the persistent-format checks |
| Retired blocks | Exact old generations superseded by committed copy transactions |
| Pending copy/recovery blocks | Valid headers with an incomplete GC copy transaction |
| Written logical sectors | Unique valid committed LBA records in current generations, out of 31744 |
| Committed records | Metadata-valid committed records, including superseded copies |
| Obsolete records | Committed records minus unique logical sectors |
| Incomplete writes | Nonempty records without a completed commit word |
| Invalid committed metadata | Committed records with invalid LBA/complement fields |
| Trailing slots | Slots after the last nonempty record in allocated blocks; not all belong to the active append block |
| Blocks needing erase | Invalid-header plus retired blocks; pending recovery is reported separately |
| Unallocated / reclaimable | Blank-header, invalid-header, and retired blocks combined |
| Free above reserve | The preceding count minus the two-block reserve, floored at zero |

These are flash allocation statistics, **not DOS filesystem free space**.
Deleting a DOS file does not immediately remove its flash-sector mappings.
A blank header does not prove that the payload is physically erased. The
normal allocator erases a block before reuse. Payload CRCs are not scanned;
this is not an every-byte memory test or an endurance/lifetime estimate.

The scanner performs chip identification, read-array restoration, header CRC
and complement checks, source-generation retirement analysis, and record
metadata scanning. It sends only Intel 90h/FFh mode commands and reads;
it never programs or erases data flash, modifies the map, or performs mount
recovery. A pending transaction is reported rather than repaired.

DOS and setup share `src/tools/flashstats.inc`. The setup scanner uses scratch
space in the existing pre-mount map allocation; the 80 KiB resident reservation
is unchanged. ROM size remains 32 KiB. Serial remains OFF by default and has
no UART access while off. BIOS 81h remains selectable without another disk.

Build checks cover 286 assembly, ROM size, signature, both checksums, and
blank factory settings pages. The metadata scan and progress display have
been manually reviewed; see setup.md for the setup controls.
