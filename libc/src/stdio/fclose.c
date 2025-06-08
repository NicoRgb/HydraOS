#include <stdio.h>
#include <stdint.h>

void syscall_close(uint64_t stream);

int fclose(FILE *f)
{
    syscall_close((uint64_t)f);
    return 0;
}
