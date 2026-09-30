#!/bin/sh
# Refresh the prebuilt DOS package from the current sources.
set -eu
root=$(CDPATH= cd -P "$(dirname "$0")" && pwd)
. "$root/scripts/rom-common.sh"
require zip
require sha256sum
sh "$root/build.sh"
mkdir -p "$root/dist"
cp "$root/bin/SLATON.BIN" "$root/bin/SLFLASH.COM" "$root/bin/SLSTAT.COM" "$root/dist/"
version=$(cat "$root/VERSION")
archive="$root/dist/SLFLASH-DOS-$version.zip"
[ ! -e "$archive" ] || rm "$archive"
(cd "$root/dist" && zip -X -q "$archive" SLATON.BIN SLFLASH.COM SLSTAT.COM README.TXT)
(cd "$root/dist" && sha256sum README.TXT SLATON.BIN SLFLASH.COM SLSTAT.COM \
    "SLFLASH-DOS-$version.zip" > SHA256SUMS)
printf 'Packaged %s\n' "$archive"
