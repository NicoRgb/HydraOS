#include <stdint.h>
#include <stddef.h>

#include <kernel/vmm.h>
#include <kernel/kprintf.h>
#include <kernel/proc/task.h>
#include <kernel/string.h>
#include <kernel/dev/devm.h>
#include <kernel/isr.h>
#include <kernel/kmm.h>

static void *process_get_pointer(process_t *proc, uintptr_t vaddr)
{
    size_t offset = (uint64_t)vaddr % PAGE_SIZE;
    uint64_t t = pml4_get_phys(proc->pml4, (void *)((vaddr / PAGE_SIZE) * PAGE_SIZE), true);
    if (t == 0)
    {
        return NULL;
    }

    return (void *)(t + offset);
}

#define DRIVER_TYPE_CHARDEV 0
#define DRIVER_TYPE_INPUTDEV 1

int64_t syscall_read(process_t *proc, int64_t stream, int64_t data, int64_t size, int64_t, int64_t, int64_t, task_state_t *)
{
    uint8_t *buf = (uint8_t *)process_get_pointer(proc, data);
    if (!buf)
    {
        return -RES_EUNKNOWN;
    }

    size_t bytes_read = 0;
    int res = stream_read(&proc->streams[stream], buf, size, &bytes_read);
    if (res < 0)
    {
        return res;
    }

    return (int64_t)bytes_read;
}

int64_t syscall_write(process_t *proc, int64_t stream, int64_t data, int64_t size, int64_t, int64_t, int64_t, task_state_t *)
{
    uint8_t *buf = (uint8_t *)process_get_pointer(proc, data);
    if (!buf)
    {
        return -RES_EUNKNOWN;
    }

    size_t bytes_written = 0;
    int res = stream_write(&proc->streams[stream], buf, size, &bytes_written);
    if (res < 0)
    {
        return res;
    }

    return (int64_t)bytes_written;
}

int64_t syscall_fork(process_t *proc, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    process_t *fork = process_clone(proc); // TODO: maybe the file changed
    if (!fork)
    {
        PANIC("failed to fork process");
    }

    fork->task.state.rax = 0; // return value

    if (process_register(fork) < 0)
    {
        PANIC("failed to register process");
    }

    return fork->pid;
}

int64_t syscall_exit(process_t *proc, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    process_unregister(proc);
    process_free(proc);
    execute_next_process();

    PANIC("failed to execute process");

    return 0;
}

int64_t syscall_ping(process_t *, int64_t pid, int64_t, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    if (get_process_from_pid((uint64_t)pid) != NULL)
    {
        return pid;
    }

    return 0;
}

int64_t syscall_exec(process_t *proc, int64_t _path, int64_t, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    const char *path = process_get_pointer(proc, (uintptr_t)_path);
    uint64_t pid = proc->pid;

    process_t *exec = process_create(path);
    if (!exec)
    {
        return -RES_EUNKNOWN;
    }

    exec->pid = pid;
    
    process_unregister(proc);
    process_free(proc);

    if (IS_ERROR(process_register(exec)))
    {
        PANIC("failed to register process");
    }

    execute_next_process();
    PANIC("failed to execute process");

    return 0;
}

int64_t syscall_alloc(process_t *proc, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    return (int64_t)process_allocate_page(proc);
}

int64_t syscall_open(process_t *proc, int64_t _path, int64_t _open_actions, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    const char *path = process_get_pointer(proc, (uintptr_t)_path);
    return (int64_t)process_insert_file(proc, path, (uint8_t)_open_actions);
}

int64_t syscall_close(process_t *proc, int64_t stream, int64_t, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    process_remove_stream(proc, (size_t)stream);
    return 0;
}

extern page_table_t *kernel_pml4;

int64_t syscall_handler(uint64_t num, int64_t arg0, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4, int64_t arg5, task_state_t *state)
{
    if (pml4_switch(kernel_pml4) < 0)
    {
        PANIC("failed to switch pml4");
        while (1);
    }

    process_t *proc = get_current_process();
    if (!proc)
    {
        PANIC("failed get current process");
        while (1);
    }

    memcpy(&proc->task.state, state, sizeof(task_state_t));

    int64_t res = -1;
    switch (num)
    {
    case 0:
        res = syscall_read(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 1:
        res = syscall_write(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 2:
        res = syscall_fork(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 3:
        res = syscall_exit(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 4:
        res = syscall_ping(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 5:
        res = syscall_exec(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 6:
        res = syscall_alloc(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 7:
        res = syscall_open(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 8:
        res = syscall_close(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    default:
        break;
    }

    if (pml4_switch(proc->pml4) < 0)
    {
        PANIC("failed to switch pml4");
    }

    return res;
}
