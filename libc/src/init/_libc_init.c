#include <stdint.h>
#include <stddef.h>

extern uint64_t _stdin;
extern uint64_t _stdout;
extern uint64_t _stderr;

int kmm_init(size_t initial_size, size_t _alignment);
void syscall_exit(uint32_t result);

void initialize_standard_library()
{
    _stdin = 0;
    _stdout = 1;
    _stderr = 2;

    if (kmm_init(8, 16) < 0)
    {
        syscall_exit(1);
    }
}
