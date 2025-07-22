#include <string.h>

char *strrchr(const char *st, int c)
{
    char *_st = NULL;
    for (size_t i = 0; st[i] != 0x00; i++)
    {
        if (st[i] == c)
        {
            _st = (char *)&st[i];
        }
    }

    return _st;
}
