#include <stdio.h>
#include <stdint.h>

size_t syscall_lseek(uint64_t stream, size_t offset, int action);

void fseek(FILE *f, size_t n, int a)
{
    syscall_lseek((uint64_t)f, n, a);
}
