#include <string.h>

void *memchr(const char *cs, int c, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        if (cs[i] == c)
        {
            return (void *)&cs[i];
        }
    }

    return NULL;
}
