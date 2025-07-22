#include <string.h>
#include <stdint.h>

char *rawmemchr(const char *st, int c)
{
    if (c != 0)
    {
        return memchr(st, c, (size_t)-1);
    }

    return (char *)((uintptr_t)st + strlen(st));
}
