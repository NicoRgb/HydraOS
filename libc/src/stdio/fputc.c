#include <stdio.h>
#include <stdint.h>

uint64_t syscall_write(uint64_t stream, const uint8_t *data, size_t size);

int fputc(char c, FILE *f)
{
    if ((int64_t)syscall_write((uint64_t)f, &c, 1) < 0)
    {
        return -1;
    }

    return c;
}
