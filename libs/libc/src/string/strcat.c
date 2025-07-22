#include <string.h>

char *strcat(char *s, const char *ct)
{
    char *_s = s;

    for (; *s; s++);
    while ((*s++ = *ct++));

    return _s;
}
