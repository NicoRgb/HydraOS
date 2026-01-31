#include <stdlib.h>

size_t free_memory(void *ptr);

void free(void *p)
{
    if (!p)
    {
        return;
    }

    free_memory(p);
}
