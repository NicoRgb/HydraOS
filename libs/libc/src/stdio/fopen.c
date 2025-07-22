#include <stdio.h>
#include <stdint.h>

uint64_t syscall_open(const uint8_t *path, uint8_t open_actions);

#define OPEN_ACTION_READ 0
#define OPEN_ACTION_WRITE 1
#define OPEN_ACTION_CLEAR 2
#define OPEN_ACTION_CREATE 3

FILE *fopen(const char *s, const char *a)
{
    if (a == NULL)
    {
        return NULL;
    }

    uint8_t open_actions = OPEN_ACTION_READ;
    switch (*a)
    {
    case 'r':
        open_actions = OPEN_ACTION_READ;
        break;
    case 'w':
        open_actions = OPEN_ACTION_WRITE;
        break;
    case 'e':
        open_actions = OPEN_ACTION_CLEAR;
        break;
    case 'c':
        open_actions = OPEN_ACTION_CREATE;
        break;
    }

    return (FILE *)syscall_open((const uint8_t *)s, open_actions);
}
