#!/bin/bash

set -e

DIR="`dirname "${BASH_SOURCE[0]}"`"
cd "$DIR" || exit

source build.sh

qemu-system-x86_64 -drive file=../hydraos.img,format=raw -debugcon file:/dev/stdout -no-shutdown -no-reboot -cpu qemu64 -display sdl -device virtio-gpu-pci -d guest_errors \
-object filter-dump,id=f1,netdev=net0,file=dump.dat \
-netdev user,id=net0 \
-device virtio-net-pci,netdev=net0
