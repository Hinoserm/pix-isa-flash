#!/bin/sh
# Build the current BIOS and DOS utilities. No hardware access.
set -eu
root=$(CDPATH= cd -P "$(dirname "$0")" && pwd)
. "$root/scripts/rom-common.sh"
for command in nasm od awk dd; do require "$command"; done
out="$root/bin"
mkdir -p "$out"
assemble() {
    nasm -w+error -f bin -I "$root/src/" -I "$out/" \
        -l "$out/${1##*/}.lst" -o "$out/$2" "$root/src/$1.asm"
}
assemble disk/core disk_core.bin
assemble disk/rom SLATON.BIN
assemble tools/slflash SLFLASH.COM
assemble tools/slstat SLSTAT.COM
rom="$out/SLATON.BIN"
[ "$(wc -c < "$rom")" -eq 32768 ] || fail 'Unexpected ROM size'
[ "$(byte_at "$rom" 16383)" -eq 255 ] || fail 'Checksum byte overlaps executable data'
sum=$(prefix_sum "$rom" 16383)
write_byte "$rom" 16383 "$(( (256 - sum) % 256 ))"
# The last 128 bytes are settings pages. Balance the full image before them.
write_byte "$rom" 32639 0
sum=$(byte_sum "$rom")
write_byte "$rom" 32639 "$(( (256 - sum) % 256 ))"
sh "$root/check.sh" "$rom"
if command -v sha256sum >/dev/null 2>&1; then
    (cd "$out" && sha256sum SLATON.BIN SLFLASH.COM SLSTAT.COM > SHA256SUMS)
fi
printf 'Built %s\n' "$out/SLATON.BIN" "$out/SLFLASH.COM" "$out/SLSTAT.COM"
