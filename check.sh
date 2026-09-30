#!/bin/sh
# Check a factory ROM's size, signature, checksums and blank settings pages.
set -eu
root=$(CDPATH= cd -P "$(dirname "$0")" && pwd)
. "$root/scripts/rom-common.sh"
for command in od awk wc; do require "$command"; done
rom=${1:-"$root/bin/SLATON.BIN"}
[ -f "$rom" ] || fail "ROM not found: $rom"
[ "$(wc -c < "$rom")" -eq 32768 ] || fail 'ROM must be exactly 32768 bytes'
[ "$(byte_at "$rom" 0)" -eq 85 ] || fail 'Invalid ROM signature'
[ "$(byte_at "$rom" 1)" -eq 170 ] || fail 'Invalid ROM signature'
[ "$(byte_at "$rom" 2)" -eq 32 ] || fail 'Declared checksum span must be 16 KiB'
[ "$(prefix_sum "$rom" 16384)" -eq 0 ] || fail 'Declared ROM checksum failed'
[ "$(byte_sum "$rom")" -eq 0 ] || fail 'Full-chip checksum failed'
od -An -v -tu1 -j 32640 -N 128 "$rom" |
    awk '{ for (i=1; i<=NF; i++) { n++; if ($i != 255) bad=1 } } END { exit (bad || n != 128) }' ||
    fail 'Factory settings pages must contain FF bytes'
printf 'PASS: 32 KiB image, 55 AA 20 header, both checksums, blank settings pages.\n'
