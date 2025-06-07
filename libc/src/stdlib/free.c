#include <stdlib.h>

void buddy_allocator_free(void *ptr);

void free(void *p)
{
    if (!p)
    {
        return;
    }

    buddy_allocator_free(p);
}
