#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void *syscall_alloc(void);

#define PAGE_SIZE 4096

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
    size_t order = 0;
    while ((size_t)(MIN_BUDDY_SIZE << order) < normalized && order < MAX_ORDER - 1)
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
    expand_size = align_forward_size(expand_size, PAGE_SIZE);

    for (size_t i = 0; i < expand_size / PAGE_SIZE; i++)
    {
        syscall_alloc();
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
        if (kmm_expand(adjusted_size) < 0)
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

int kmm_init(size_t initial_size, size_t _alignment)
{
    if ((initial_size & (initial_size - 1)) != 0 || (_alignment & (_alignment - 1)) != 0)
    {
        return -1;
    }

    if (_alignment < sizeof(buddy_header_t))
    {
        _alignment = sizeof(buddy_header_t);
    }

    alignment = _alignment;
    void *base = syscall_alloc();

    initial_size = align_forward_size(initial_size, PAGE_SIZE);
    for (size_t i = 1; i < initial_size / PAGE_SIZE; i++)
    {
        syscall_alloc();
    }

    if (((uintptr_t)base % _alignment) != 0)
    {
        return -1;
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
