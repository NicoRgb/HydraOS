#ifndef _KERNEL_KERNEL_H
#define _KERNEL_KERNEL_H

#include <kernel/log.h>
#include <kernel/multiboot2.h>
#include <kernel/string.h>
#include <kernel/proc/elf.h>

#define MAX_MMAP_ENTRIES 20

#define MMAP_ENTRY_TYPE_AVAILABLE 1 << 0
#define MMAP_ENTRY_TYPE_RESERVED 1 << 1
#define MMAP_ENTRY_TYPE_ACPI_RECLAIMABLE 1 << 2

typedef struct
{
    uint64_t addr;
    uint64_t size;
    uint8_t type;
} memory_map_entry_t;

#define MAX_MMAP_ENTRIES 20
#define MAX_ELF_SECTIONS 64

typedef struct
{
    uint8_t tty;
    uint64_t total_memory;
    uint64_t num_mmap_entries;
    memory_map_entry_t memory_map[MAX_MMAP_ENTRIES];
    uint64_t num_elf_sections;
    uint32_t shndx;
    uint32_t symtabndx;
    Elf64_Shdr elf_sections[MAX_ELF_SECTIONS];
} boot_info_t;

extern boot_info_t boot_info;

#endif