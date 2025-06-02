ROOT ?= ./

build/sysinit: sysinit.c $(ROOT)/lib/libc.a $(ROOT)/lib/libhydra.a
	mkdir -p build

	x86_64-elf-gcc -g -T ./linker.ld -o $@ -ffreestanding -O0 -nostdlib -fpic -g sysinit.c $(ROOT)/lib/libc.a $(ROOT)/lib/libhydra.a -I $(ROOT)/include -static -nostartfiles

.PHONY: all
all: build/sysinit
