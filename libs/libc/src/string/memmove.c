#include <string.h>
#include <stdint.h>

void *memmove(char *s, const char *ct, size_t n)
{
    if (s == ct || n == 0)
    {
        return s;
    }

    if (s > ct && s - ct < (int)n)
    {
        for (int64_t i = n - 1; i >= 0; i--)
        {
            s[i] = ct[i];
        }
        return s;
    }

    if (ct > s && ct - s < (int)n)
    {
        for (size_t i = 0; i < n; i++)
        {
            s[i] = ct[i];
        }
        return s;
    }

    memcpy(s, ct, n);
    return s;
}
