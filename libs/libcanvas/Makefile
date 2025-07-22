OBJS:=$(patsubst src/%.c, build/%.o, $(wildcard $(shell find src -name '*.c')))
OBJS+=$(patsubst src/%.asm, build/%.asm.o, $(wildcard $(shell find src -name '*.asm')))

ARCH:=x86_64

CFLAGS:=-Wall -Wextra -nostdlib -ffreestanding -O0 -g
ASFLAGS:=

CC:=$(ARCH)-elf-gcc
AS:=nasm
AR:=$(ARCH)-elf-ar

.PHONY: all
all: build/libcanvas.a

build/%.o: src/%.c
	@mkdir -p $(@D)
	@echo "CC\t$@"
	@$(CC) $^ -o $@ -c $(CFLAGS) -Iinclude -I../libc/include

build/%.asm.o: src/%.asm
	@mkdir -p $(@D)
	@echo "AS\t$@"
	@nasm -f elf64 -o $@ $^

build/libcanvas.a: $(OBJS)
	@mkdir -p $(@D)
	@echo "AR\t$@"
	@$(AR) rcs $@ $^
