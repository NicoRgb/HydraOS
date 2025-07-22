#include <string.h>

int strcmp(const char *st, const char *ct)
{
    while (*st == *ct++)
    {
        if (!*st++)
        {
            return 0;
        }
    }

    return *(const unsigned char *)st - *(const unsigned char *)(ct - 1);
}
