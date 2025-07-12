#!/bin/bash

set -e

DIR="`dirname "${BASH_SOURCE[0]}"`"
cd "$DIR" || exit

HYDRAOS_BOOT_SYSTEM='GRUB'

source build.sh

if [[ $HYDRAOS_BOOT_SYSTEM == 'UEFI' ]]; then
    qemu-system-x86_64 -drive file=../hydraos.img,format=raw -debugcon file:/dev/stdout -no-shutdown -no-reboot -cpu qemu64 -drive if=pflash,format=raw,unit=0,file="../extern/OVMF/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="../extern/OVMF/OVMF_VARS-pure-efi.fd" -net none
else
    qemu-system-x86_64 -drive file=../hydraos.img,format=raw -debugcon file:/dev/stdout -no-shutdown -no-reboot -cpu qemu64 -device virtio-gpu-pci -device virtio-rng-pci -device virtio-net-pci -display sdl -d guest_errors
fi
