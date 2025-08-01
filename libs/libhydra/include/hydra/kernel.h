#ifndef _KERNEL_H
#define _KERNEL_H 1

#include <stdint.h>
#include <stddef.h>

#define _SYSCALL_READ 0
#define _SYSCALL_WRITE 1
#define _SYSCALL_FORK 2
#define _SYSCALL_EXIT 3
#define _SYSCALL_PING 4
#define _SYSCALL_EXEC 5
#define _SYSCALL_ALLOC 6
#define _SYSCALL_OPEN 7
#define _SYSCALL_CLOSE 8
#define _SYSCALL_PIPE 12
#define _SYSCALL_LSEEK 13

#define RES_SUCCESS 0
#define RES_INVARG 1
#define RES_OVERFLOW 2
#define RES_CORRUPT 4
#define RES_NOMEM 5
#define RES_UNAVAILABLE 6
#define RES_TIMEOUT 7
#define RES_ACCESS_DENIED 8

#define RES_EUNKNOWN 10
#define RES_ETEST 11

uint64_t syscall(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);

uint64_t syscall_read(uint64_t stream, uint8_t *data, size_t size);
uint64_t syscall_write(uint64_t stream, const uint8_t *data, size_t size);
uint64_t syscall_fork(void);
void syscall_exit(uint32_t result);
uint64_t syscall_ping(uint64_t pid);

typedef struct
{
    const char **args;
    size_t num_args;
    
    const char **envars;
    size_t num_envars;

    uint64_t stdin_idx;
    uint64_t stdout_idx;
    uint64_t stderr_idx;
} __attribute__((packed)) process_create_info_t;

void syscall_exec(const uint8_t *path, process_create_info_t *create_info);

void *syscall_alloc(void);
uint64_t syscall_open(const uint8_t *path, uint8_t open_actions);
void syscall_close(uint64_t stream);
int syscall_pipe(void);
size_t syscall_lseek(uint64_t stream, size_t offset, int action);

#endif