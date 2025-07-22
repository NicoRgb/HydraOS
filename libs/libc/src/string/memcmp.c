#include <string.h>

int memcmp(const char *cs, const char *ct, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        if (cs[i] != ct[i])
        {
            return cs[i] < ct[i] ? -1 : 1;
        }
    }

    return 0;
}
