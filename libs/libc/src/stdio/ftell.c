#include <stdio.h>
#include <stdint.h>

size_t syscall_lseek(uint64_t stream, size_t offset, int action);

size_t ftell(FILE *f)
{
    return syscall_lseek((uint64_t)f, 0, SEEK_CUR);
}
