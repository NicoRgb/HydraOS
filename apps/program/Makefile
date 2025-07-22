ROOT ?= ./

build/program: program.c stb_image.c $(ROOT)/lib/libc.a $(ROOT)/lib/libhydra.a $(ROOT)/lib/libcanvas.a
	mkdir -p build

	x86_64-elf-gcc -g -T ./linker.ld -o $@ -ffreestanding -O0 -nostdlib -fpic -g $^ -I $(ROOT)/include -static -nostartfiles

.PHONY: all
all: build/program
