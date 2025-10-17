add-symbol-file kernel/build/kernel.elf
add-symbol-file apps/sysinit/build/sysinit
add-symbol-file apps/shell/build/shell
add-symbol-file apps/program/build/program
target remote | qemu-system-x86_64 -S -gdb stdio -debugcon file:kernel.log -drive file=hydraos.img,format=raw -no-shutdown -no-reboot -cpu qemu64 -d guest_errors -monitor telnet:127.0.0.1:1234,server,nowait \
    -display sdl -device virtio-gpu-pci -d guest_errors \
    -object filter-dump,id=f1,netdev=net0,file=dump.dat \
    -netdev user,id=net0 \
    -device virtio-net-pci,netdev=net0
