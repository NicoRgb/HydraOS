#include <kernel/pmm.h>
#include <kernel/string.h>
#include <stdbool.h>

struct
{
    uint64_t *bitmap;
    uint64_t num_pages;
    uint64_t last_index;
    uint64_t max_addr;
} page_allocator;

static void bit_set(uint64_t *bitmap, uint64_t index)
{
    uint64_t array_index = index / 64;
    uint64_t bit_offset = index % 64;
    bitmap[array_index] |= (1UL << bit_offset);
}

static void bit_clear(uint64_t *bitmap, uint64_t index)
{
    uint64_t array_index = index / 64;
    uint64_t bit_offset = index % 64;
    bitmap[array_index] &= ~(1UL << bit_offset);
}

static bool bit_get(const uint64_t *bitmap, uint64_t index)
{
    uint64_t array_index = index / 64;
    uint64_t bit_offset = index % 64;
    return (bitmap[array_index] & (1UL << bit_offset)) != 0;
}

int pmm_init(memory_map_entry_t *memory_map, uint64_t num_mmap_entries, uint64_t total_memory)
{
    int res = 0;
    memset(&page_allocator, 0, sizeof(page_allocator));

    page_allocator.num_pages = total_memory / PAGE_SIZE;
    page_allocator.max_addr = memory_map[num_mmap_entries - 1].addr + memory_map[num_mmap_entries - 1].size;
    page_allocator.bitmap = (uint64_t *)page_allocator.max_addr; // needed for validation... nullptr is a possible value here, max_addr isn't

    uint64_t bitmap_size = (page_allocator.num_pages + 8 - 1) / 8;
    for (uint64_t i = 0; i < num_mmap_entries; i++)
    {
        if (memory_map[i].type != MMAP_ENTRY_TYPE_AVAILABLE)
        {
            continue;
        }

        if (memory_map[i].size < bitmap_size)
        {
            continue;
        }

        page_allocator.bitmap = (uint64_t *)memory_map[i].addr;
        memory_map[i].addr += bitmap_size;
        memory_map[i].size -= bitmap_size;
        break;
    }

    if ((uint64_t)page_allocator.bitmap == page_allocator.max_addr)
    {
        res = -1;
        goto out;
    }

    memset(page_allocator.bitmap, 0xFF, bitmap_size); // mark everything as reserved/used
    for (uint64_t i = 0; i < num_mmap_entries; i++)
    {
        if (memory_map[i].type != MMAP_ENTRY_TYPE_AVAILABLE)
        {
            continue;
        }

        uint64_t start_page = (memory_map[i].addr + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint64_t j = 0; j < memory_map[i].size / PAGE_SIZE; j++)
        {
            uint64_t page_index = start_page + j;
            bit_clear(page_allocator.bitmap, page_index);
        }
    }

out:
    return res;
}

void pmm_reserve(uint64_t *page)
{
    bit_set(page_allocator.bitmap, (uintptr_t)page / PAGE_SIZE);
}

void *pmm_alloc(void)
{
    for (uint64_t i = page_allocator.last_index; i < page_allocator.num_pages; i++)
    {
        if (bit_get(page_allocator.bitmap, i))
            continue;

        bit_set(page_allocator.bitmap, i);
        page_allocator.last_index = i;

        return (void *)(i * PAGE_SIZE);
    }

    PANIC("page allocation failed");
    return NULL;
}

void pmm_free(uint64_t *page)
{
    uint64_t index = (uint64_t)page / PAGE_SIZE;
    bit_clear(page_allocator.bitmap, index);
    page_allocator.last_index = index;
}

uint64_t get_max_addr(void)
{
    return page_allocator.max_addr;
}

void slab_init(slab_allocator_t *slab, void *page)
{
    slab->address = (uintptr_t)page;
    slab->max_address = (uintptr_t)page + PAGE_SIZE;
}

void *slab_alloc(slab_allocator_t *slab, uint8_t size)
{
    if (slab->address + size >= slab->max_address)
    {
        return NULL;
    }

    void *addr = (void *)slab->address;
    slab->address += size;
    return addr;
}
