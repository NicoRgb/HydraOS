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
    int res = stream_read(proc->streams[stream], buf, size, &bytes_read);
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
    int res = stream_write(proc->streams[stream], buf, size, &bytes_written);
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

int64_t syscall_exec(process_t *proc, int64_t _path, int64_t _create_info, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    process_create_info_t *create_info = (process_create_info_t *)process_get_pointer(proc, (uintptr_t)_create_info);

    const char *path = process_get_pointer(proc, (uintptr_t)_path);
    const char **args = (const char **)process_get_pointer(proc, (uintptr_t)create_info->args);
    const char **envars = (const char **)process_get_pointer(proc, (uintptr_t)create_info->envars);
    uint64_t pid = proc->pid;

    process_t *exec = process_create(path);
    if (!exec)
    {
        return -RES_EUNKNOWN;
    }

    char **arguments = kmalloc(create_info->num_args * sizeof(char *));
    if (!arguments)
    {
        PANIC("out of kernel heap memory");
    }

    for (uint16_t i = 0; i < create_info->num_args; i++)
    {
        arguments[i] = strdup(process_get_pointer(proc, (uintptr_t)args[i]));
        if (!arguments[i])
        {
            PANIC("out of kernel heap memory");
        }
    }

    char **environment_variables = kmalloc(create_info->num_envars * sizeof(char *));
    if (!environment_variables)
    {
        PANIC("out of kernel heap memory");
    }

    for (uint16_t i = 0; i < create_info->num_envars; i++)
    {
        environment_variables[i] = strdup(process_get_pointer(proc, (uintptr_t)envars[i]));
        if (!environment_variables[i])
        {
            PANIC("out of kernel heap memory");
        }
    }

    if (process_set_args(exec, arguments, create_info->num_args) < 0)
    {
        return -RES_EUNKNOWN;
    }

    if (process_set_envars(exec, environment_variables, create_info->num_envars) < 0)
    {
        return -RES_EUNKNOWN;
    }

    if (process_set_stdin(exec, proc->streams[create_info->stdin_idx]) < 0)
    {
        return -RES_EUNKNOWN;
    }

    if (process_set_stdout(exec, proc->streams[create_info->stdout_idx]) < 0)
    {
        return -RES_EUNKNOWN;
    }

    if (process_set_stderr(exec, proc->streams[create_info->stderr_idx]) < 0)
    {
        return -RES_EUNKNOWN;
    }

    if (setup_initial_stack(exec) < 0)
    {
        return -RES_EUNKNOWN;
    }

    exec->pid = pid;
    
    if (process_unregister(proc) < 0)
    {
        return -RES_EUNKNOWN;
    }

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

int64_t syscall_video_get_display_rect(process_t *proc, int64_t display_id, int64_t _rect, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    video_rect_t *rect = process_get_pointer(proc, (uintptr_t)_rect);
    if (!rect)
    {
        return -RES_INVARG;
    }

    device_t *dev = get_device_by_type(DEVICE_VIDEO, 0);
    if (!dev)
    {
        return -RES_INVARG;
    }

    int status = device_get_display_rect(rect, display_id, dev);
    return status;
}

int64_t syscall_video_create_framebuffer(process_t *proc, int64_t display_id, int64_t _rect, int64_t _vaddr, int64_t, int64_t, int64_t, task_state_t *)
{
    video_rect_t *rect = process_get_pointer(proc, (uintptr_t)_rect);
    if (!rect)
    {
        return -RES_INVARG;
    }

    device_t *dev = get_device_by_type(DEVICE_VIDEO, 0);
    if (!dev)
    {
        return -RES_INVARG;
    }

    uint32_t *fb = device_create_framebuffer(rect, display_id, dev);
    if (!fb)
    {
        return -RES_EUNKNOWN;
    }

    size_t num_pages = (get_framebuffer_size(rect) + PAGE_SIZE - 1) / PAGE_SIZE;
    if (_vaddr < 0x900000 || _vaddr > 0x1000000)
    {
        return -RES_ACCESS_DENIED;
    }

    for (size_t i = 0; i < num_pages; i++)
    {
        if (pml4_get_phys(proc->pml4, (void *)((uintptr_t)_vaddr + PAGE_SIZE * i), false) != 0)
        {
            return -RES_ACCESS_DENIED;
        }
    }

    int status = pml4_map_range(proc->pml4, (void *)_vaddr, fb, num_pages, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    return status;
}

int64_t syscall_video_update_display(process_t *proc, int64_t _fb, int64_t _rect, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    video_rect_t *rect = process_get_pointer(proc, (uintptr_t)_rect);
    if (!rect)
    {
        return -RES_INVARG;
    }

    uint32_t *fb = process_get_pointer(proc, (uintptr_t)_fb);
    if (!fb)
    {
        return -RES_INVARG;
    }

    device_t *dev = get_device_by_type(DEVICE_VIDEO, 0);
    if (!dev)
    {
        return -RES_INVARG;
    }

    int status = device_update_display(rect, fb, dev);
    return status;
}

int64_t syscall_pipe(process_t *proc, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, task_state_t *)
{
    stream_t *stream = stream_create_bidirectional(0);
    if (!stream)
    {
        return 0;
    }
    
    return process_insert_stream(proc, stream);
}

int64_t syscall_lseek(process_t *proc, int64_t stream, int64_t offset, int64_t action, int64_t, int64_t, int64_t, task_state_t *)
{
    stream_t *s = proc->streams[stream];
    if (!stream)
    {
        return 0;
    }

    if (s->type != STREAM_TYPE_FILE)
    {
        return 0;
    }

    vfs_seek(s, (size_t)offset, (uint8_t)action);
    
    return s->node->offset;
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

        // TODO: this is a temporary api -> it will be replaced by a /dev directory with one syscall that can interact with mounted devices
    case 9:
        res = syscall_video_get_display_rect(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 10:
        res = syscall_video_create_framebuffer(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 11:
        res = syscall_video_update_display(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
        
    case 12:
        res = syscall_pipe(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
        break;
    case 13:
        res = syscall_lseek(proc, arg0, arg1, arg2, arg3, arg4, arg5, state);
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
