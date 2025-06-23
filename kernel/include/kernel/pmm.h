#ifndef _KERNEL_PMM_H
#define _KERNEL_PMM_H

#include <stdint.h>
#include <kernel/status.h>
#include <kernel/kernel.h>

#define PAGE_SIZE 4096

int pmm_init(memory_map_entry_t *memory_map, uint64_t num_mmap_entries, uint64_t total_memory);
void pmm_reserve(uint64_t *page);
void *pmm_alloc(void);
void pmm_free(uint64_t *page);

uint64_t get_max_addr(void);

typedef struct
{
    uintptr_t address;
    uintptr_t max_address;
} slab_allocator_t;

void slab_init(slab_allocator_t *slab, void *page);
void *slab_alloc(slab_allocator_t *slab, uint8_t size);

#endif