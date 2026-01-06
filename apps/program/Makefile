ROOT ?= ./

build/program: program.c $(ROOT)/lib/libcanvas.a $(ROOT)/lib/libc.a $(ROOT)/lib/libhydra.a
	@mkdir -p build

	@x86_64-elf-gcc -g -T ./linker.ld -o $@ -ffreestanding -O0 -nostdlib -fpic -g $^ -I $(ROOT)/include -static -nostartfiles

.PHONY: all
all: build/program
