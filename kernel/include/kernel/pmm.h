#ifndef _KERNEL_PMM_H
#define _KERNEL_PMM_H

#include <stdint.h>
#include <kernel/status.h>
#include <kernel/kernel.h>

#define PAGE_SIZE 4096

int pmm_init(memory_map_entry_t *memory_map, uint64_t num_mmap_entries, uint64_t total_memory);
void *pmm_alloc(void);
void pmm_free(uint64_t *page);

uint64_t get_max_addr(void);

#endif