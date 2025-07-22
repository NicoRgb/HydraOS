#include <string.h>

void *memcpy(char *s, const char *ct, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        s[i] = ct[i];
    }

    return s;
}
