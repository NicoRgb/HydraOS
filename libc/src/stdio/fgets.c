#include <stdio.h>
#include <stdint.h>

char *fgets(char *s, int c, FILE *f)
{
    int _c = 0;
    while (_c < (c - 1))
    {
        char ascii = fgetc(f);
        if (ascii == 0)
        {
            continue;
        }
        else if (ascii == '\n' || ascii < 0)
        {
            break;
        }

        s[_c++] = 0;
    }

    s[_c] = 0;
    return s;
}
