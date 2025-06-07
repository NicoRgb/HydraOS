#include <kernel/kmm.h>
#include <kernel/pmm.h>
#include <kernel/string.h>
#include <stdbool.h>

#define MIN_BUDDY_SIZE 64
#define EXPAND_PAGE_NUM 16

#define BUDDY_FLAG_USED 1

#define MAX_ORDER 16

typedef struct buddy_header
{
    size_t size;
    uint8_t flags;
    struct buddy_header *next;
    struct buddy_header *prev;
} buddy_header_t;

buddy_header_t *free_lists[MAX_ORDER] = {0};

buddy_header_t *get_next_buddy(buddy_header_t *header)
{
    return (buddy_header_t *)((uintptr_t)header + header->size);
}

buddy_header_t *get_prev_buddy(buddy_header_t *header)
{
    return (buddy_header_t *)((uintptr_t)header - header->size);
}

buddy_header_t *mark_as_used(buddy_header_t *buddy)
{
    if (buddy == NULL)
    {
        return NULL;
    }

    buddy->flags |= BUDDY_FLAG_USED;
    return buddy;
}

bool buddy_is_free(buddy_header_t *buddy)
{
    return (buddy->flags & BUDDY_FLAG_USED) != BUDDY_FLAG_USED;
}

int size_to_order(size_t size)
{
    size_t normalized = size < MIN_BUDDY_SIZE ? MIN_BUDDY_SIZE : size;
    int order = 0;
    while ((MIN_BUDDY_SIZE << order) < normalized && order < MAX_ORDER - 1)
    {
        order++;
    }
    return order;
}

size_t order_to_size(int order)
{
    return MIN_BUDDY_SIZE << order;
}

void insert_free_buddy(buddy_header_t *buddy)
{
    int order = size_to_order(buddy->size);

    if (buddy == buddy->next)
    {
        PANIC("self-loop detected in free list insertion: %p", buddy);
    }

    buddy->next = free_lists[order];
    buddy->prev = NULL;
    if (free_lists[order])
    {
        free_lists[order]->prev = buddy;
    }
    free_lists[order] = buddy;
}

void remove_free_buddy(buddy_header_t *buddy)
{
    int order = size_to_order(buddy->size);
    if (buddy->prev)
    {
        buddy->prev->next = buddy->next;
    }
    else
    {
        free_lists[order] = buddy->next;
    }
    if (buddy->next)
    {
        buddy->next->prev = buddy->prev;
    }
    buddy->next = buddy->prev = NULL;
}

page_table_t *heap_pml4 = NULL;
buddy_header_t *head = NULL;
buddy_header_t *tail = NULL;
uint8_t alignment = 8;

buddy_header_t *split_buddy(buddy_header_t *buddy, size_t target_size)
{
    if (buddy == NULL)
    {
        return NULL;
    }

    while (buddy->size / 2 >= target_size && buddy->size / 2 >= MIN_BUDDY_SIZE)
    {
        size_t half = buddy->size / 2;
        buddy->size = half;

        buddy_header_t *split = get_next_buddy(buddy);
        split->size = half;
        split->flags = 0;

        insert_free_buddy(split);
    }
    return buddy;
}

void buddy_coalesce(buddy_header_t *buddy)
{
    while (true)
    {
        buddy_header_t *next = get_next_buddy(buddy);
        if (next == tail)
        {
            break;
        }

        if (buddy->size == next->size && buddy_is_free(buddy) && buddy_is_free(next))
        {
            remove_free_buddy(next);
            buddy->size *= 2;
        }
        else
        {
            break;
        }
    }
}

buddy_header_t *allocate_buddy(size_t size)
{
    int target_order = size_to_order(size);

    for (int order = target_order; order < MAX_ORDER; order++)
    {
        if (free_lists[order] != NULL)
        {
            buddy_header_t *buddy = free_lists[order];
            remove_free_buddy(buddy);
            buddy = split_buddy(buddy, order_to_size(target_order));
            buddy->flags |= BUDDY_FLAG_USED;
            return buddy;
        }
    }

    return NULL; // out of memory
}

void free_buddy(buddy_header_t *buddy)
{
    if (!buddy)
    {
        return;
    }

    if (buddy_is_free(buddy))
    {
        PANIC("double free detected at %p", buddy);
        return;
    }

    buddy->flags &= ~BUDDY_FLAG_USED;

    buddy_coalesce(buddy);

    insert_free_buddy(buddy);
}

size_t align_forward_size(size_t ptr, size_t align)
{
    size_t a, p, modulo;

    a = align;
    p = ptr;
    modulo = p & (a - 1);
    if (modulo != 0)
    {
        p += a - modulo;
    }
    return p;
}

int kmm_expand(size_t expand_size)
{
    LOG_INFO("allocator expanded");

    expand_size = align_forward_size(expand_size, PAGE_SIZE);
    if (expand_size * PAGE_SIZE < EXPAND_PAGE_NUM)
    {
        expand_size = EXPAND_PAGE_NUM * PAGE_SIZE;
    }

    for (size_t i = 0; i < expand_size / PAGE_SIZE; i++)
    {
        void *page = pmm_alloc();
        if (!page)
        {
            return -RES_NOMEM;
        }

        int status = pml4_map(heap_pml4, (void *)(tail + i * PAGE_SIZE), page, PAGE_PRESENT | PAGE_WRITABLE);
        if (status < 0)
        {
            return status;
        }
    }

    buddy_header_t *new_node = (buddy_header_t *)tail;
    new_node->size = expand_size;
    new_node->flags = 0;

    tail = get_next_buddy(new_node);

    insert_free_buddy(new_node);
    buddy_coalesce(head);

    return 0;
}

void *buddy_allocator_alloc(size_t size)
{
    size_t adjusted_size = size + alignment;
    buddy_header_t *allocation = allocate_buddy(adjusted_size);

    if (allocation == NULL)
    {
        if (IS_ERROR(kmm_expand(adjusted_size)))
        {
            return NULL;
        }

        allocation = allocate_buddy(adjusted_size);

        if (allocation == NULL)
        {
            return NULL;
        }
    }

    return (void *)((uintptr_t)allocation + alignment);
}

void buddy_allocator_free(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    buddy_header_t *buddy = (buddy_header_t *)((uintptr_t)ptr - alignment);
    free_buddy(buddy);
}

int kmm_init(page_table_t *kernel_pml4, uint64_t base, size_t initial_size, size_t _alignment)
{
    if (!kernel_pml4 || base == 0 || (initial_size & (initial_size - 1)) != 0 || (_alignment & (_alignment - 1)) != 0)
    {
        return -RES_INVARG;
    }

    if (_alignment < sizeof(buddy_header_t))
    {
        _alignment = sizeof(buddy_header_t);
    }

    if (((uintptr_t)base % _alignment) != 0)
    {
        return -RES_INVARG;
    }

    alignment = _alignment;
    heap_pml4 = kernel_pml4;

    for (size_t i = 0; i < initial_size / PAGE_SIZE; i++)
    {
        void *page = pmm_alloc();
        if (!page)
        {
            return -RES_NOMEM;
        }

        int status = pml4_map(kernel_pml4, (void *)(base + i * PAGE_SIZE), page, PAGE_PRESENT | PAGE_WRITABLE);
        if (status < 0)
        {
            return status;
        }
    }

    head = (buddy_header_t *)base;
    head->size = initial_size;
    head->flags = 0;
    head->next = head->prev = NULL;

    tail = get_next_buddy(head);

    int initial_order = size_to_order(initial_size);
    free_lists[initial_order] = head;

    return 0;
}

void *kmalloc(size_t size)
{
    if (size >= PAGE_SIZE)
    {
        LOG_WARNING("buddy allocation greather than one page, size %lld", size);
    }

    void *res = buddy_allocator_alloc(size);
    if (res == NULL)
    {
        PANIC("no more heap in kernel");
    }
    return res;
}

void kfree(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    buddy_allocator_free(ptr);
}

void *krealloc(void *ptr, size_t old_size, size_t new_size)
{
    void *res = kmalloc(new_size);
    memcpy(res, ptr, old_size);
    kfree(ptr);
    return res;
}

void visualize_buddy_tree(void)
{
    LOG_INFO("Buddy Allocator Tree:");
    LOG_INFO("------------------------------------");

    for (buddy_header_t *buddy = head; buddy != tail; buddy = get_next_buddy(buddy))
    {
        size_t size = buddy->size;
        size_t max_size = (uintptr_t)tail - (uintptr_t)head;
        int level = 0;
        for (size_t s = size; s < max_size; s <<= 1)
        {
            level++;
        }

        char indent[32];
        memset(indent, ' ', level * 2);
        indent[level * 2] = '\0';

        const char *used_str = buddy_is_free(buddy) ? "[Free]" : "[Used]";
        LOG_INFO("%s├─ Addr: 0x%08llx | Size: %-8llu %s",
                 indent,
                 (uint64_t)(uintptr_t)buddy,
                 (uint64_t)buddy->size,
                 used_str);
    }

    LOG_INFO("------------------------------------");
}

void print_free_lists(void)
{
    for (int i = 0; i < MAX_ORDER; i++)
    {
        LOG_INFO("Order %d (Size %zu):", i, order_to_size(i));
        for (buddy_header_t *b = free_lists[i]; b; b = b->next)
        {
            LOG_INFO("  Addr: %p, Size: %zu", b, b->size);

            if (b == b->next)
            {
                PANIC("self-loop detected in free list insertion: %p", b);
            }
        }
    }
}

size_t get_free_memory(void)
{
    size_t size = 0;
    for (buddy_header_t *buddy = head; buddy != tail; buddy = get_next_buddy(buddy))
    {
        size += buddy->size;
    }

    return size;
}

size_t get_frag_count(void)
{
    size_t count = 0;
    for (buddy_header_t *buddy = head; buddy != tail; buddy = get_next_buddy(buddy))
    {
        if (buddy_is_free(buddy))
        {
            count++;
        }
    }

    return count;
}
