add-symbol-file kernel/build/kernel.elf
add-symbol-file apps/sysinit/build/sysinit
add-symbol-file apps/shell/build/shell
add-symbol-file apps/program/build/program
target remote | qemu-system-x86_64 -S -gdb stdio --no-shutdown -drive file=hydraos.img,format=raw -monitor telnet:127.0.0.1:1234,server,nowait
