#include <kernel/string.h>
#include <kernel/kmm.h>
#include <stdbool.h>

#define LONG_MAX ((long)(~0UL >> 1))
#define LONG_MIN (~LONG_MAX)

size_t strlen(const char *str)
{
    const char *s;

    for (s = str; *s; ++s)
        ;

    return (s - str);
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const uint8_t *)s1 - *(const uint8_t *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0)
        return (0);
    do
    {
        if (*s1 != *s2++)
            return (*(unsigned char *)s1 - *(unsigned char *)--s2);
        if (*s1++ == 0)
            break;
    } while (--n != 0);
    return (0);
}

char *strcpy(char *to, char *from)
{
    size_t i = 0;
    for (i = 0; from[i] != 0x00; i++)
    {
        to[i] = from[i];
    }

    to[i] = 0;

    return to;
}

void *memset(void *dest, register int val, register size_t len)
{
    register unsigned char *ptr = (unsigned char *)dest;
    while (len-- > 0)
    {
        *ptr++ = val;
    }
    return dest;
}

void memcpy(void *dest, const void *src, size_t len)
{
    char *csrc = (char *)src;
    char *cdest = (char *)dest;

    for (size_t i = 0; i < len; i++)
    {
        cdest[i] = csrc[i];
    }
}

int memcmp(const char *cs_in, const char *ct_in, size_t n)
{
    size_t i;
    const unsigned char *cs = (const unsigned char *)cs_in;
    const unsigned char *ct = (const unsigned char *)ct_in;

    for (i = 0; i < n; i++, cs++, ct++)
    {
        if (*cs < *ct)
        {
            return -1;
        }
        else if (*cs > *ct)
        {
            return 1;
        }
    }
    return 0;
}

int atoui(char *s)
{
    int acum = 0;
    while ((*s >= '0') && (*s <= '9'))
    {
        acum = acum * 10;
        acum = acum + (*s - 48);
        s++;
    }
    return (acum);
}

int isdigit(char c)
{
    return c >= '0' && c <= '9';
}

int isspace(char c)
{
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

int islower(char c)
{
    return c >= 'a' && c <= 'z';
}

int isupper(char c)
{
    return c >= 'A' && c <= 'Z';
}

int isalpha(char c)
{
    return islower(c) || isupper(c);
}

long strtol(const char *nPtr, char **endPtr, int base)
{
    if ((base < 2 || base > 36) && base != 0)
    {
        return 0;
    }

    long number = 0;
    const char *divider;
    int currentdigit, sign, cutlim;
    enum
    {
        NEGATIVE,
        POSITIVE
    };
    unsigned long cutoff;
    bool correctconversion = true;

    divider = nPtr;

    while (isspace(*divider))
    {
        divider++;
    }

    if (*divider == '+')
    {
        sign = POSITIVE;
        divider++;
    }
    else if (*divider == '-')
    {
        sign = NEGATIVE;
        divider++;
    }
    else
    {
        sign = POSITIVE;
    }

    if (*divider == '\0')
    {
        if (endPtr != NULL)
        {
            *endPtr = (char *)divider;
        }
        return 0;
    }

    if (base == 0)
    {
        if (*divider == '0')
        {
            divider++;
            if (*divider == 'x' || *divider == 'X')
            {
                base = 16;
                divider++;
            }
            else
            {
                base = 8;
            }
        }
        else
        {
            base = 10;
        }
    }
    else if (base == 16 && *divider == '0' && (divider[1] == 'x' || divider[1] == 'X'))
    {
        divider += 2;
    }

    if (sign == POSITIVE)
    {
        cutoff = LONG_MAX / (unsigned long)base;
    }
    else
    {
        cutoff = -(unsigned long)LONG_MIN / (unsigned long)base;
    }
    cutlim = cutoff % (unsigned long)base;

    // Convert characters to number
    while (*divider != '\0')
    {
        if (isdigit(*divider))
        {
            currentdigit = *divider - '0';
        }
        else if (isalpha(*divider))
        {
            if (islower(*divider))
            {
                currentdigit = *divider - 'a' + 10;
            }
            else
            {
                currentdigit = *divider - 'A' + 10;
            }
        }
        else
        {
            break;
        }

        if (currentdigit >= base)
        {
            break;
        }

        if (!correctconversion ||
            number > (long)cutoff ||
            (number == (long)cutoff && currentdigit > cutlim))
        {
            correctconversion = false;
        }
        else
        {
            correctconversion = true;
            number = number * base + currentdigit;
        }

        divider++;
    }

    if (!correctconversion)
    {
        if (sign == POSITIVE)
        {
            number = LONG_MAX;
        }
        else
        {
            number = LONG_MIN;
        }
    }
    else if (sign == NEGATIVE)
    {
        number = -number;
    }

    if (endPtr != NULL)
    {
        *endPtr = (char *)divider;
    }

    return number;
}

char *strchr(const char *p, int ch)
{
    char c;

    c = ch;
    for (;; ++p)
    {
        if (*p == c)
            return ((char *)p);
        if (*p == '\0')
            return (NULL);
    }

    return NULL;
}

void *memrchr(const void *s, int c, size_t n)
{
    const unsigned char *sp = (const unsigned char *)s + n - 1;

    while (n--)
    {
        if (*sp == (unsigned char)c)
            return (void *)sp;
        sp--;
    }

    return NULL;
}

bool is_delim(char c, char *delim)
{
    while (*delim != '\0')
    {
        if (c == *delim)
            return true;
        delim++;
    }
    return false;
}

char *strtok(char *s, char *delim)
{
    static char *p;
    if (!s)
    {
        s = p;
    }
    if (!s)
    {
        return NULL;
    }

    while (1)
    {
        if (is_delim(*s, delim))
        {
            s++;
            continue;
        }
        if (*s == '\0')
        {
            return NULL;
        }
        break;
    }

    char *ret = s;
    while (1)
    {
        if (*s == '\0')
        {
            p = s;
            return ret;
        }
        if (is_delim(*s, delim))
        {
            *s = '\0';
            p = s + 1;
            return ret;
        }
        s++;
    }
}

char *strrchr(const char *str, int ch)
{
    char *result = NULL;
    while (*str != '\0')
    {
        if (*str == ch)
            result = (char *)str;
        str++;
    }
    if (ch == '\0')
        result = (char *)str;
    return result;
}

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

char *strdup(const char *s1)
{
    char *str;
    size_t size = strlen(s1) + 1;

    str = kmalloc(size);
    if (str)
    {
        memcpy(str, s1, size);
    }
    return str;
}

int toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        return c - ('a' - 'A');
    return c;
}

int isalnum(int c)
{
    return isalpha(c) || (c >= '0' && c <= '9');
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    size_t i;
    for (i = 0; i < n && *s != '\0'; i++)
        *d++ = *s++;
    for (; i < n; i++)
        *d++ = '\0';
    return dest;
}
