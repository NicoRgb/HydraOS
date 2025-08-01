ROOT ?= ./

KOBJS:=$(patsubst src/%.c, build/%.o, $(wildcard $(shell find src -name '*.c')))
KOBJS+=$(patsubst src/%.asm, build/%.asm.o, $(wildcard $(shell find src -name '*.asm')))

ARCH:=x86_64

LIBGCC:=$(shell dirname `find /home/ubuntu/opt/cross/ -name "libgcc.a"`)

CFLAGS:=-Wall -Wextra -std=c99 -nostdlib -ffreestanding -O0 -g -fstack-protector -fno-omit-frame-pointer
ASFLAGS:=
LDFLAGS:=-n -m elf_$(ARCH) --no-dynamic-linker -nostdlib -z max-page-size=0x1000 --build-id=none -static -L$(LIBGCC) -lgcc

CC:=$(ARCH)-elf-gcc
AS:=nasm
LD:=$(ARCH)-elf-ld

CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTI_OBJ:=build/arch/$(ARCH)/crti.asm.o
CRTN_OBJ:=build/arch/$(ARCH)/crtn.asm.o

KOBJS:=$(filter-out $(CRTI_OBJ) $(CRTN_OBJ), $(KOBJS))

.PHONY: all, clean
all: build/kernel.elf

clean:
	@echo "RM\tbuild"
	@rm -rf build | true

build/%.o: src/%.c
	@mkdir -p $(@D)
	@echo "CC\t$@"
	@$(CC) $^ -o $@ -c $(CFLAGS) -Iinclude

build/%.asm.o: src/%.asm
	@mkdir -p $(@D)
	@echo "AS\t$@"
	@nasm -f elf64 -o $@ $^

build/kernel.elf: $(CRTI_OBJ) $(CRTBEGIN_OBJ) $(KOBJS) $(ROOT)/mod/fat32_module.a $(ROOT)/mod/mbr_module.a $(ROOT)/mod/ide_module.a $(ROOT)/mod/virtio_video_module.a $(ROOT)/mod/virtio_net_module.a $(ROOT)/mod/virtio_entropy_module.a $(CRTEND_OBJ) $(CRTN_OBJ)
	@mkdir -p $(@D)
	@echo "LD\t$@"
	@$(LD) --whole-archive $^ --no-whole-archive -o $@ -T src/arch/$(ARCH)/linker.ld $(LDFLAGS)
	@grub-file --is-x86-multiboot2 $@
	@grub-file --is-x86-multiboot $@
