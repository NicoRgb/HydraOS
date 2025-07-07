#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void *realloc(void *p, size_t old_size, size_t s)
{
    void *res = malloc(s);
    memcpy(res, p, old_size);
    free(p);
    return res;
}
