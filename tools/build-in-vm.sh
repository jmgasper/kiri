#!/usr/bin/env bash
set -euo pipefail
KIRI_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$KIRI_ROOT"
tar -czf - CMakeLists.txt Makefile README.md LICENSE src tests tools resources vendor docs |
    bash tools/haiku.sh 'mkdir -p /boot/home/kiri && tar xzf - -C /boot/home/kiri'
bash tools/haiku.sh 'cd /boot/home/kiri && make -j4 && make check'
mkdir -p artifacts
scp -O -i .vm/id_ed25519 -P 2224 -o IdentitiesOnly=yes -o BatchMode=yes \
    -o UserKnownHostsFile=.vm/known_hosts \
    user@127.0.0.1:/boot/home/kiri/build-haiku/Kiri artifacts/Kiri
