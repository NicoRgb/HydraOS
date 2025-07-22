#include <string.h>

size_t strlen(const char *cs)
{
    size_t len = 0;
    while (*cs)
    {
        len++;
        cs++;
    }

    return len;
}
