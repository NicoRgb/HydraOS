#include <stdio.h>
#include <stdint.h>

uint64_t syscall_write(uint64_t stream, const uint8_t *data, size_t size);
size_t strlen(const char *cs);

int fputs(const char *st, FILE *f)
{
    return (int)syscall_write((uint64_t)f, st, strlen(st));
}
