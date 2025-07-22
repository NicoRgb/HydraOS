#include <string.h>

size_t strspn(const char *st, const char *ct)
{
    const char *p;
    const char *a;
    size_t c = 0;

    for (p = st; *p != 0x00; p++)
    {
        for (a = ct; *a != 0x00; a++)
        {
            if (*p == *a)
            {
                break;
            }
        }

        if (*a == 0x00)
        {
            return c;
        }

        c++;
    }

    return c;
}
