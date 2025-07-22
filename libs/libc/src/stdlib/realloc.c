#include <stdlib.h>
#include <string.h>
#include <stdint.h>

size_t ptr_get_size(void *ptr);

void *realloc(void *p, size_t s)
{
    if (!p)
    {
        return malloc(s);
    }

    void *res = malloc(s);
    memcpy(res, p, ptr_get_size(res));
    free(p);
    return res;
}
