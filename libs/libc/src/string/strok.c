#include <string.h>

char *rawmemchr(const char *st, int c);

static char *_s;

char *strtok(char *s, const char *ct)
{
    char *tok;

    if (!s)
    {
        s = _s;
    }

    s += strspn(s, ct);
    if (*s == 0x00)
    {
        _s = s;
        return NULL;
    }

    tok = s;
    s = strpbrk(s, ct);
    if (s == NULL)
    {
        _s = rawmemchr(tok, 0x00);
        return tok;
    }

    *s = 0x00;
    _s = s + 1;
    return tok;
}
