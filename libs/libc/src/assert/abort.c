#include <stdint.h>

void syscall_exit(uint32_t result);

void abort(void)
{
    syscall_exit(1);
}
