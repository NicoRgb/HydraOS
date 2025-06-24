ROOT ?= ./

OBJS:=$(patsubst src/%.c, build/%.o, $(wildcard $(shell find src -name '*.c')))
OBJS+=$(patsubst src/%.asm, build/%.asm.o, $(wildcard $(shell find src -name '*.asm')))

ARCH:=x86_64

CFLAGS:=-Wall -Wextra -std=c99 -nostdlib -ffreestanding -O0 -g -fstack-protector -fno-omit-frame-pointer
ASFLAGS:=

CC:=$(ARCH)-elf-gcc
AS:=nasm

.PHONY: all, clean
all: build/virtio_video_module.a

clean:
	@echo "RM\tbuild"
	@rm -rf build | true

build/%.o: src/%.c
	@mkdir -p $(@D)
	@echo "CC\t$@"
	@$(CC) $^ -o $@ -c $(CFLAGS) -Iinclude -I$(ROOT)/include

build/%.asm.o: src/%.asm
	@mkdir -p $(@D)
	@echo "AS\t$@"
	@nasm -f elf64 -o $@ $^

build/virtio_video_module.a: $(OBJS)
	@mkdir -p $(@D)
	@echo "AR\t$@"
	@ar rcs $@ $^
