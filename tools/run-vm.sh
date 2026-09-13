#!/usr/bin/env bash
set -euo pipefail
KIRI_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$KIRI_ROOT"
if [[ ! -f .vm/work.qcow2 ]]; then
    echo 'Missing .vm/work.qcow2. See docs/VM.md for the test VM setup.' >&2
    exit 1
fi
if python3 tools/vm.py status >/dev/null 2>&1; then
    echo 'The Kiri VM is already running.'
    exit 0
fi
exec qemu-system-x86_64 -enable-kvm -cpu host -m 4096 -smp 4 \
    -drive file=.vm/work.qcow2,format=qcow2,if=ide,index=0 \
    -nic user,model=e1000,hostfwd=tcp:127.0.0.1:2224-:22 \
    -device qemu-xhci -device usb-tablet -vga std -display none \
    -vnc 127.0.0.1:4 -qmp unix:.vm/qmp.sock,server=on,wait=off \
    -pidfile .vm/qemu.pid -serial file:.vm/serial.log -daemonize
