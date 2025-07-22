#include <string.h>

char *strchr(const char *st, int c)
{
    for (size_t i = 0; st[i] != 0x00; i++)
    {
        if (st[i] == c)
        {
            return (void *)&st[i];
        }
    }

    return NULL;
}
