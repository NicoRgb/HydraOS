#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

// TODO: implement slab allocators for small allocations

typedef struct
{
    uint64_t offset;
    uint64_t size;
} buddy_t;

typedef struct
{
    uint64_t min_alloc_size;
    uint64_t max_alloc_size;
    uint64_t total_size;
} buddy_allocator_params_t;

typedef struct _buddy_allocator buddy_allocator_t;

uint64_t get_buddy_allocator_metadata_size(buddy_allocator_params_t params);
buddy_allocator_t *buddy_allocator_init(buddy_allocator_params_t params, void *metadata);
buddy_t buddy_alloc(buddy_allocator_t *b, uint64_t size);
uint64_t buddy_free(buddy_allocator_t *b, uint64_t offset);
uint64_t buddy_get_size(buddy_allocator_t *b, uint64_t offset);

#define BUDDY_ALLOCATOR_MIN_ALLOCATION 64
#define BUDDY_ALLOCATOR_MAX_ALLOCATION (64 * 1024)
#define BUDDY_ALLOCATOR_ARENA_SIZE (1024 * 1024)
#define MAX_HEAP_SIZE (64 * 1024 * 1024)

#define MAX_ARENA (MAX_HEAP_SIZE / BUDDY_ALLOCATOR_ARENA_SIZE)

uint32_t num_buddy_allocators;
buddy_allocator_t *buddy_arenas[MAX_ARENA];
void *buddy_arena_bases[MAX_ARENA];

static uint32_t last_arena;

static buddy_allocator_t *large_arena = NULL;
static void *large_arena_base = NULL;

#define LARGE_ALLOCATION_THRESHOLD (64 * 1024)
#define LARGE_ALLOCATION_MAX 16 * 1024 * 1024

#define PAGE_SIZE 4096
void *syscall_alloc(void);

void allocator_init(void)
{
    last_arena = 0;

    buddy_allocator_params_t params = {.max_alloc_size = BUDDY_ALLOCATOR_MAX_ALLOCATION,
                                       .min_alloc_size = BUDDY_ALLOCATOR_MIN_ALLOCATION,
                                       .total_size = BUDDY_ALLOCATOR_ARENA_SIZE};

    uint64_t metadata_size = get_buddy_allocator_metadata_size(params);
    void *metadata = syscall_alloc();
    for (size_t i = 0; i < (metadata_size + PAGE_SIZE - 1) / PAGE_SIZE - 1; i++)
    {
        syscall_alloc();
    }

    assert(metadata);

    num_buddy_allocators = 1;
    buddy_arenas[0] = buddy_allocator_init(params, metadata);
    buddy_arena_bases[0] = syscall_alloc();
    for (size_t i = 0; i < (BUDDY_ALLOCATOR_ARENA_SIZE + PAGE_SIZE - 1) / PAGE_SIZE - 1; i++)
    {
        syscall_alloc();
    }

    assert(buddy_arena_bases[0]);

    buddy_allocator_params_t large_params = {
        .max_alloc_size = LARGE_ALLOCATION_MAX,
        .min_alloc_size = LARGE_ALLOCATION_THRESHOLD,
        .total_size = LARGE_ALLOCATION_MAX
    };

    uint64_t large_metadata_size = get_buddy_allocator_metadata_size(large_params);
    void *large_metadata = syscall_alloc();
    for (size_t i = 0; i < (large_metadata_size + PAGE_SIZE - 1) / PAGE_SIZE - 1; i++)
    {
        syscall_alloc();
    }
    assert(large_metadata);

    large_arena = buddy_allocator_init(large_params, large_metadata);
    large_arena_base = syscall_alloc();
    for (size_t i = 0; i < (large_params.total_size + PAGE_SIZE - 1) / PAGE_SIZE - 1; i++)
    {
        syscall_alloc();
    }
    assert(large_arena_base);
}

void *allocate_memory(size_t size)
{
    if (size == 0 || size > LARGE_ALLOCATION_MAX)
    {
        return NULL;
    }

    if (size >= LARGE_ALLOCATION_THRESHOLD)
    {
        buddy_t buddy = buddy_alloc(large_arena, size);
        if (buddy.size >= size)
        {
            return (void *)((uintptr_t)large_arena_base + buddy.offset);
        }
        return NULL; // out of memory in large arena
    }

    uint32_t current_arena;
    buddy_t buddy = {0, 0};

    for (current_arena = last_arena; current_arena < num_buddy_allocators; current_arena++)
    {
        buddy = buddy_alloc(buddy_arenas[current_arena], size);
        if (buddy.size >= size)
        {
            break;
        }
    }

    if (buddy.size >= size)
    {
        last_arena = current_arena;
        return (void *)(buddy.offset + (uintptr_t)(buddy_arena_bases[current_arena]));
    }

    if (num_buddy_allocators >= MAX_ARENA)
    {
        // out of memory
        return NULL;
    }

    buddy_allocator_params_t params = {.max_alloc_size = BUDDY_ALLOCATOR_MAX_ALLOCATION,
                                       .min_alloc_size = BUDDY_ALLOCATOR_MIN_ALLOCATION,
                                       .total_size = BUDDY_ALLOCATOR_ARENA_SIZE};

    uint64_t metadata_size = get_buddy_allocator_metadata_size(params);
    void *metadata = syscall_alloc();
    for (size_t i = 0; i < (metadata_size + PAGE_SIZE - 1) / PAGE_SIZE - 1; i++)
    {
        syscall_alloc();
    }
    assert(metadata);

    uint32_t new_arena = num_buddy_allocators;
    buddy_arenas[new_arena] = buddy_allocator_init(params, metadata);
    buddy_arena_bases[new_arena] = syscall_alloc();
    for (size_t i = 0; i < (BUDDY_ALLOCATOR_ARENA_SIZE + PAGE_SIZE - 1) / PAGE_SIZE - 1; i++)
    {
        syscall_alloc();
    }
    assert(buddy_arena_bases[new_arena]);
    num_buddy_allocators++;

    buddy = buddy_alloc(buddy_arenas[new_arena], size);
    if (buddy.size >= size)
    {
        last_arena = new_arena;
        return (void *)(buddy.offset + (uintptr_t)buddy_arena_bases[new_arena]);
    }

    return NULL;
}

size_t free_memory(void *ptr)
{
    if (!ptr)
    {
        return 0;
    }

    if ((uintptr_t)ptr >= (uintptr_t)large_arena_base &&
        (uintptr_t)ptr < (uintptr_t)large_arena_base + LARGE_ALLOCATION_MAX)
    {
        uint64_t offset = (uintptr_t)ptr - (uintptr_t)large_arena_base;
        return buddy_free(large_arena, offset);
    }

    for (uint32_t i = 0; i < num_buddy_allocators; i++)
    {
        uintptr_t base = (uintptr_t)buddy_arena_bases[i];
        uintptr_t p = (uintptr_t)ptr;

        if (p >= base && p < base + BUDDY_ALLOCATOR_ARENA_SIZE)
        {
            assert(((p - base) & (BUDDY_ALLOCATOR_MIN_ALLOCATION - 1)) == 0);

            if (last_arena > i)
            {
                last_arena = i;
            }

            uint64_t offset = p - base;
            return buddy_free(buddy_arenas[i], offset);
        }
    }

    assert(0);
    return 0;
}

size_t ptr_get_size(void *ptr)
{
    if (!ptr)
    {
        return 0;
    }

    if ((uintptr_t)ptr >= (uintptr_t)large_arena_base &&
        (uintptr_t)ptr < (uintptr_t)large_arena_base + LARGE_ALLOCATION_MAX)
    {
        uint64_t offset = (uintptr_t)ptr - (uintptr_t)large_arena_base;
        return buddy_get_size(large_arena, offset);
    }

    for (uint32_t i = 0; i < num_buddy_allocators; i++)
    {
        uintptr_t base = (uintptr_t)buddy_arena_bases[i];
        uintptr_t p = (uintptr_t)ptr;

        if (p >= base && p < base + BUDDY_ALLOCATOR_ARENA_SIZE)
        {
            assert(((p - base) & (BUDDY_ALLOCATOR_MIN_ALLOCATION - 1)) == 0);

            if (last_arena > i)
            {
                last_arena = i;
            }

            uint64_t offset = p - base;
            return buddy_get_size(buddy_arenas[i], offset);
        }
    }

    assert(0);
    return 0;
}
