# Shared POSIX shell helpers for the 32 KiB AT29C257 image.
fail() {
    printf '%s\n' "$*" >&2
    exit 1
}
require() {
    command -v "$1" >/dev/null 2>&1 || fail "Required command not found: $1"
}
byte_at() {
    od -An -tu1 -j "$2" -N 1 "$1" | awk '{ print $1 }'
}
byte_sum() {
    od -An -v -tu1 "$1" | awk '{ for (i = 1; i <= NF; i++) sum += $i } END { print sum % 256 }'
}
prefix_sum() {
    od -An -v -tu1 -N "$2" "$1" | awk '{ for (i = 1; i <= NF; i++) sum += $i } END { print sum % 256 }'
}
write_byte() {
    octal=$(printf '\\0%03o' "$3")
    printf '%b' "$octal" | dd of="$1" bs=1 seek="$2" conv=notrunc 2>/dev/null
}
