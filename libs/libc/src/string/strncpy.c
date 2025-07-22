#include <string.h>

char *strncpy(char *s, const char *ct, size_t n)
{
    size_t i = 0;
    for (i = 0; i < n && ct[i] != 0x00; i++)
    {
        s[i] = ct[i];
    }

    s[i] = 0;

    return s;
}
