#include <stdio.h>
#include <stdint.h>

uint64_t syscall_read(uint64_t stream, uint8_t *data, size_t size);

int fgetc(FILE *f)
{
    uint8_t res;
    if ((int64_t)syscall_read((uint64_t)f, &res, 1) < 0)
    {
        return -1;
    }

    return res;
}
