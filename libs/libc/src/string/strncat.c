#include <string.h>

char *strncat(char *s, const char *ct, size_t n)
{
    size_t l1 = strlen(s);
    size_t l2 = strlen(ct);

    if (l2 < n)
    {
        strcpy(&s[l1], ct);
    }
    else
    {
        strncpy(&s[l1], ct, n);
        s[l1 + n] = '\0';
    }

    return s;
}
