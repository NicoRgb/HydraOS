#include <stdlib.h>

void *buddy_allocator_alloc(size_t size);

void *malloc(size_t s)
{
    return buddy_allocator_alloc(s);
}
