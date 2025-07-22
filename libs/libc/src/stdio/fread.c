#include <stdio.h>
#include <stdint.h>

uint64_t syscall_read(uint64_t stream, uint8_t *data, size_t size);

size_t fread(char *s, size_t n, size_t u, FILE *f)
{
    int64_t res = (int64_t)syscall_read((uint64_t)f, (uint8_t *)s, n * u);
    if (res < 0)
    {
        res = 0;
    }
    return res;
}
