#include <stdio.h>
#include <stdint.h>

uint64_t syscall_write(uint64_t stream, const uint8_t *data, size_t size);

size_t fwrite(const char *st, size_t n, size_t u, FILE *f)
{
    int64_t res = (int64_t)syscall_write((uint64_t)f, (const uint8_t *)st, n * u);
    if (res < 0)
    {
        res = 0;
    }
    return res;
}
