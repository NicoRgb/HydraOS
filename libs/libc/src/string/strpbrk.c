#include <string.h>

char *strpbrk(const char *st, const char *ct)
{
    while (*st != 0x00)
    {
        const char *a = ct;
        while (*a != 0x00)
        {
            if (*a++ == *st)
            {
                return (char *)st;
            }
        }

        st++;
    }

    return NULL;
}
