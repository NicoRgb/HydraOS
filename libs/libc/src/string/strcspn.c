#include <string.h>

size_t strcspn(const char *st, const char *ct)
{
    size_t n = 0;
    const char *_ct = ct;

    while (*st)
    {
        for (_ct = ct; *_ct; _ct++)
        {
            if (*st == *_ct)
            {
                return n;
            }
        }
        st++;
        n++;
    }

    return n;
}
