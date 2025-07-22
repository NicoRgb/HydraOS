#include <hydra/kernel.h>

uint64_t syscall(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    uint64_t result;
    asm volatile(
        "syscall"
        :   "=a"(result)
        :   "a"(num),
            "D"(arg1),
            "S"(arg2),
            "d"(arg3),
            "r"(arg5),
            "r"(arg6),
            "r"(arg4)
        : "rcx", "r11", "memory"
    );

    return result;
}

uint64_t syscall_read(uint64_t stream, uint8_t *data, size_t size)
{
    return syscall(_SYSCALL_READ, stream, (uint64_t)data, size, 0, 0, 0);
}

uint64_t syscall_write(uint64_t stream, const uint8_t *data, size_t size)
{
    return syscall(_SYSCALL_WRITE, stream, (uint64_t)data, size, 0, 0, 0);
}

uint64_t syscall_fork(void)
{
    return syscall(_SYSCALL_FORK, 0, 0, 0, 0, 0, 0);
}

void syscall_exit(uint32_t result)
{
    syscall(_SYSCALL_EXIT, result, 0, 0, 0, 0, 0);
}

uint64_t syscall_ping(uint64_t pid)
{
    return syscall(_SYSCALL_PING, pid, 0, 0, 0, 0, 0);
}

void syscall_exec(const uint8_t *path, process_create_info_t *create_info)
{
    syscall(_SYSCALL_EXEC, (uint64_t)path, (uint64_t)create_info, 0, 0, 0, 0);
}
 
void *syscall_alloc(void)
{
    return (void *)syscall(_SYSCALL_ALLOC, 0, 0, 0, 0, 0, 0);
}

uint64_t syscall_open(const uint8_t *path, uint8_t open_actions)
{
    return syscall(_SYSCALL_OPEN, (uint64_t)path, (uint64_t)open_actions, 0, 0, 0, 0);
}

void syscall_close(uint64_t stream)
{
    syscall(_SYSCALL_CLOSE, stream, 0, 0, 0, 0, 0);
}

int syscall_pipe(void)
{
    return syscall(_SYSCALL_PIPE, 0, 0, 0, 0, 0, 0);
}

size_t syscall_lseek(uint64_t stream, size_t offset, int action)
{
    return syscall(_SYSCALL_LSEEK, (uint64_t)stream, (uint64_t)offset, (uint64_t)action, 0, 0, 0);
}
