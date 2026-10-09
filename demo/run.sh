#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make -s
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
${CC:-cc} -o "$work/sample" demo/sample.c

printf '%s\n' '--- Shelf Steam ---'
printf 'ls\nsample hello\nsample stdin < demo/input.txt\nexit\n' | ./bin/shelf-steam "$work"
printf '%s\n' '--- Parallel Hashing ---'
./bin/parallel-hashing demo/passwords.txt demo/hashes.txt "$work/results.txt" 4
cat "$work/results.txt"
printf '%s\n' '--- RAID 5 (hex disk data) ---'
./bin/raid5 2 17 demo/input.hex 8 "$work/disk1" "$work/disk2" "$work/disk3" "$work/disk4"
for disk in 1 2 3 4; do
    printf 'disk%s: ' "$disk"
    cat "$work/disk$disk"
    printf '\n'
done
python3 - "$work" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
disks = [bytes.fromhex((path / f"disk{i}").read_text()) for i in range(1, 5)]
for stripe in range(3):
    parity = 3 - stripe
    at = stripe * 2
    expected = bytes(disks[0][at + b] ^ disks[1][at + b] ^
                     disks[2][at + b] ^ disks[3][at + b] for b in range(2))
    assert expected == bytes(2), "parity does not match"
    print(f"stripe {stripe}: parity on disk {parity + 1}, XOR verified")
PY
