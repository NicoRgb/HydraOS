#include <string.h>

char *strstr(const char *st, const char *ct)
{
    const char *p = st;
    const size_t len = strlen(ct);

    if (len <= 0)
    {
        return (char *)p;
    }

    for (; (p = strchr(p, *ct)) != 0x00; p++)
    {
        if (strncmp(p, ct, len) == 0)
        {
            return (char *)p;
        }
    }

    return 0;
}
