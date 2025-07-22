#include <string.h>

int strncmp(const char *st, const char *ct, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        if (*st != *ct++)
        {
            return *(const unsigned char *)st - *(const unsigned char *)(ct - 1);
        }
        if (!*st++)
        {
            return 0;
        }
    }

    return 0;
}
