#include <string.h>

char *strcpy(char *s, const char *ct)
{
    size_t i = 0;
    for (i = 0; ct[i] != 0x00; i++)
    {
        s[i] = ct[i];
    }

    s[i] = 0;

    return s;
}
