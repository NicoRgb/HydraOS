#include <stdlib.h>

void *allocate_memory(size_t size);

void *malloc(size_t s)
{
    return allocate_memory(s);
}
