#include <kernel/proc/task.h>
#include <kernel/kmm.h>
#include <kernel/string.h>
#include <kernel/kprintf.h>
#include <kernel/pmm.h>

extern int __kernel_start;
extern int __kernel_end;

static uint64_t current_pid = 0;
process_t *process_create(const char *path)
{
    process_t *proc = kmalloc(sizeof(process_t));
    if (!proc)
    {
        return NULL;
    }

    proc->num_heap_pages = 0;

    memset(proc, 0, sizeof(process_t));

    proc->elf = elf_load(path);
    if (!proc->elf)
    {
        process_free(proc);
        return NULL;
    }

    memset(&proc->task.state, 0, sizeof(task_state_t));
    proc->task.state.rip = elf_entry(proc->elf);

    strncpy(proc->path, path, MAX_PATH);
    proc->pml4 = pmm_alloc();
    memset(proc->pml4, 0, PAGE_SIZE);
    if (!proc->pml4)
    {
        process_free(proc);
        return NULL;
    }

    for (uintptr_t i = (uintptr_t)&__kernel_start; i < (uintptr_t)&__kernel_end; i += PAGE_SIZE)
    {
        if (pml4_map(proc->pml4, (void *)i, (void *)i, PAGE_PRESENT | PAGE_WRITABLE) < 0)
        {
            process_free(proc);
            return NULL;
        }
    }

    proc->task.num_stack_pages = PROCESS_STACK_SIZE / PAGE_SIZE;
    proc->task.stack_pages = kmalloc(proc->task.num_stack_pages * sizeof(void *));
    if (!proc->task.stack_pages)
    {
        process_free(proc);
        return NULL;
    }

    for (size_t i = 0; i < proc->task.num_stack_pages; i++)
    {
        proc->task.stack_pages[i] = pmm_alloc();
        if (!proc->task.stack_pages[i])
        {
            process_free(proc);
            return NULL;
        }

        if (pml4_map(proc->pml4, (void *)(PROCESS_STACK_VADDR_BASE + (PROCESS_STACK_SIZE - i * PAGE_SIZE)), proc->task.stack_pages[i], PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) < 0)
        {
            process_free(proc);
            return NULL;
        }
    }

    if (elf_load_and_map(proc, proc->elf) < 0)
    {
        process_free(proc);
        return NULL;
    }

    memset(proc->streams, 0, PROCESS_MAX_STREAMS * sizeof(stream_t *));

    proc->task.state.rsp = PROCESS_STACK_VADDR_BASE + PROCESS_STACK_SIZE - 16;
    proc->next = NULL;

    proc->pid = current_pid++;

    elf_free(proc->elf);
    proc->elf = NULL;

    return proc;
}

process_t *process_clone(process_t *_proc)
{
    process_t *proc = kmalloc(sizeof(process_t));
    if (!proc)
    {
        return NULL;
    }

    proc->num_heap_pages = _proc->num_heap_pages;

    memset(proc, 0, sizeof(process_t));

    proc->elf = elf_load(_proc->path);
    if (!proc->elf)
    {
        process_free(proc);
        return NULL;
    }

    memcpy(&proc->task.state, &_proc->task.state, sizeof(task_state_t));

    strncpy(proc->path, _proc->path, MAX_PATH);
    proc->pml4 = pmm_alloc();
    memset(proc->pml4, 0, PAGE_SIZE);
    if (!proc->pml4)
    {
        process_free(proc);
        return NULL;
    }

    for (uintptr_t i = (uintptr_t)&__kernel_start; i < (uintptr_t)&__kernel_end; i += PAGE_SIZE)
    {
        if (pml4_map(proc->pml4, (void *)i, (void *)i, PAGE_PRESENT | PAGE_WRITABLE) < 0)
        {
            process_free(proc);
            return NULL;
        }
    }

    proc->task.num_stack_pages = _proc->task.num_stack_pages;
    proc->task.stack_pages = kmalloc(proc->task.num_stack_pages * sizeof(void *));
    if (!proc->task.stack_pages)
    {
        process_free(proc);
        return NULL;
    }

    for (size_t i = 0; i < proc->task.num_stack_pages; i++)
    {
        proc->task.stack_pages[i] = pmm_alloc();
        if (!proc->task.stack_pages[i])
        {
            process_free(proc);
            return NULL;
        }

        memcpy(proc->task.stack_pages[i], _proc->task.stack_pages[i], PAGE_SIZE);

        if (pml4_map(proc->pml4, (void *)(PROCESS_STACK_VADDR_BASE + (PROCESS_STACK_SIZE - i * PAGE_SIZE)), proc->task.stack_pages[i], PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) < 0)
        {
            process_free(proc);
            return NULL;
        }
    }

    for (size_t i = 0; i < _proc->num_heap_pages; i++)
    {
        proc->heap_pages[i] = pmm_alloc();
        if (!proc->heap_pages[i])
        {
            process_free(proc);
            return NULL;
        }

        memcpy(proc->heap_pages[i], _proc->heap_pages[i], PAGE_SIZE);

        if (pml4_map(proc->pml4, (void *)(PROCESS_HEAP_VADDR_BASE + (i * PAGE_SIZE)), proc->heap_pages[i], PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) < 0)
        {
            process_free(proc);
            return NULL;
        }
    }

    if (elf_load_and_map_copy(proc, proc->elf, _proc) < 0)
    {
        process_free(proc);
        return NULL;
    }

    memset(proc->streams, 0, PROCESS_MAX_STREAMS * sizeof(stream_t *));
    for (uint64_t i = 0; i < PROCESS_MAX_STREAMS; i++)
    {
        if (_proc->streams[i] != NULL)
        {
            proc->streams[i] = stream_clone(_proc->streams[i]);
            if (!proc->streams[i])
            {
                process_free(proc);
                return NULL;
            }
        }
    }

    proc->next = NULL;
    proc->pid = current_pid++;

    elf_free(proc->elf);
    proc->elf = NULL;

    return proc;
}

int process_set_args(process_t *proc, char **args, uint16_t num_args)
{
    proc->num_arguments = num_args;
    proc->arguments = args;

    return RES_SUCCESS;
}

int process_set_envars(process_t *proc, char **envars, uint16_t num_envars)
{
    proc->num_envars = num_envars;
    proc->envars = envars;

    return RES_SUCCESS;
}

int process_set_stdin(process_t *proc, stream_t *stdin)
{
    if (proc->streams[0] != NULL)
    {
        return -RES_EUNKNOWN;
    }

    proc->streams[0] = stream_clone(stdin);
    if (!proc->streams[0])
    {
        process_free(proc);
        return -RES_EUNKNOWN;
    }
    return RES_SUCCESS;
}

int process_set_stdout(process_t *proc, stream_t *stdout)
{
    if (proc->streams[1] != NULL)
    {
        return -RES_EUNKNOWN;
    }

    proc->streams[1] = stream_clone(stdout);
    if (!proc->streams[1])
    {
        process_free(proc);
        return -RES_EUNKNOWN;
    }
    return RES_SUCCESS;
}

int process_set_stderr(process_t *proc, stream_t *stderr)
{
    if (proc->streams[2] != NULL)
    {
        return -RES_EUNKNOWN;
    }

    proc->streams[2] = stream_clone(stderr);
    if (!proc->streams[2])
    {
        process_free(proc);
        return -RES_EUNKNOWN;
    }
    return RES_SUCCESS;
}

int setup_initial_stack(process_t *proc)
{
    uint8_t *stack_top = (uint8_t *)pml4_get_phys(proc->pml4, (void *)proc->task.state.rsp, true);
    uint8_t *sp = stack_top;

    char **argv_pointers = (char **)kmalloc(proc->num_arguments * sizeof(char *));

    for (int i = proc->num_arguments - 1; i >= 0; i--)
    {
        size_t len = strlen(proc->arguments[i]) + 1;
        sp -= len;
        sp = (uint8_t *)((uintptr_t)sp & ~0xF);
        memcpy(sp, proc->arguments[i], len);
        argv_pointers[i] = (char *)(proc->task.state.rsp - ((uint64_t)stack_top - (uint64_t)sp));
    }

    char **envar_pointers = (char **)kmalloc(proc->num_arguments * sizeof(char *));

    for (int i = proc->num_envars - 1; i >= 0; i--)
    {
        size_t len = strlen(proc->envars[i]) + 1;
        sp -= len;
        sp = (uint8_t *)((uintptr_t)sp & ~0xF);
        memcpy(sp, proc->envars[i], len);
        envar_pointers[i] = (char *)(proc->task.state.rsp - ((uint64_t)stack_top - (uint64_t)sp));
    }

    sp -= sizeof(int);
    *(int *)sp = proc->num_arguments;

    for (int i = proc->num_arguments - 1; i >= 0; i--)
    {
        sp -= sizeof(char *);
        *(char **)sp = argv_pointers[i];
    }

    char **argv_start = (char **)(proc->task.state.rsp - ((uint64_t)stack_top - (uint64_t)sp));

    sp -= sizeof(char *);
    *(char **)sp = NULL;

    for (int i = proc->num_envars - 1; i >= 0; i--)
    {
        sp -= sizeof(char *);
        *(char **)sp = envar_pointers[i];
    }

    char **envp_start = (char **)(proc->task.state.rsp - ((uint64_t)stack_top - (uint64_t)sp));

    sp -= sizeof(char *);
    *(char **)sp = NULL;

    proc->task.state.rsp -= (uint64_t)stack_top - (uint64_t)sp;
    proc->task.state.rdi = proc->num_arguments;
    proc->task.state.rsi = (uint64_t)argv_start;
    proc->task.state.rdx = proc->num_envars;
    proc->task.state.rcx = (uint64_t)envp_start;

    return RES_SUCCESS;
}

void *process_allocate_page(process_t *proc)
{
    size_t index = proc->num_heap_pages++;
    void *virt = (void *)(PROCESS_HEAP_VADDR_BASE + (index * PAGE_SIZE));

    if (index > PROCESS_MAX_HEAP_PAGES)
    {
        PANIC("process reached heap limit");
        return NULL;
    }

    proc->heap_pages[index] = pmm_alloc();
    if (proc->heap_pages[index] == NULL)
    {
        return NULL;
    }

    if (pml4_map(proc->pml4, virt, proc->heap_pages[index], PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) < 0)
    {
        PANIC("page mapping failed");
        return NULL;
    }

    return virt;
}

size_t process_insert_stream(process_t *proc, stream_t *stream)
{
    if (!stream)
    {
        return 0;
    }

    for (size_t i = 0; i < PROCESS_MAX_STREAMS; i++)
    {
        if (proc->streams[i] == NULL)
        {
            proc->streams[i] = stream;
            return i;
        }
    }

    return 0;
}

size_t process_insert_file(process_t *proc, const char *path, uint8_t open_action)
{
    for (size_t i = 0; i < PROCESS_MAX_STREAMS; i++)
    {
        if (proc->streams[i] == NULL)
        {
            proc->streams[i] = vfs_open(path, open_action);
            if (!proc->streams[i])
            {
                return 0;
            }
            return i;
        }
    }

    return 0;
}

void process_remove_stream(process_t *proc, size_t index)
{
    stream_free(proc->streams[index]);
    proc->streams[index] = NULL;
}

void process_free(process_t *proc)
{
    if (!proc)
    {
        return;
    }

    if (proc->arguments != NULL)
    {
        for (uint16_t i = 0; i < proc->num_arguments; i++)
        {
            kfree(proc->arguments[i]);
        }
        kfree(proc->arguments);
    }

    if (proc->elf)
    {
        elf_free(proc->elf);
    }
    if (proc->pml4)
    {
        pmm_free((uint64_t *)proc->pml4);
    }
    if (proc->data_pages)
    {
        for (size_t i = 0; i < proc->num_data_pages; i++)
        {
            pmm_free(proc->data_pages[i]);
        }
        kfree(proc->data_pages);
    }
    if (proc->task.stack_pages)
    {
        for (size_t i = 0; i < proc->task.num_stack_pages; i++)
        {
            pmm_free(proc->task.stack_pages[i]);
        }
        kfree(proc->task.stack_pages);
    }

    for (int i = 0; i < PROCESS_MAX_STREAMS; i++)
    {
        if (proc->streams[i] != NULL)
        {
            stream_free(proc->streams[i]);
        }
    }

    kfree(proc);
}

process_t *proc_head = NULL;
process_t *current_proc = NULL;

int process_register(process_t *proc)
{
    proc->next = NULL;
    if (!proc_head)
    {
        proc_head = proc;
        return 0;
    }

    process_t *tail = NULL;
    for (tail = proc_head; tail->next != NULL; tail = tail->next)
        ;
    if (!tail)
    {
        return -RES_CORRUPT;
    }

    tail->next = proc;

    return 0;
}

int process_unregister(process_t *proc)
{
    if (!proc_head)
    {
        return -RES_CORRUPT;
    }

    if (current_proc == proc)
    {
        current_proc = proc_head;
        if (proc_head == proc)
        {
            current_proc = proc_head->next;
        }
    }

    if (proc_head == proc)
    {
        proc_head = proc->next;
        return 0;
    }

    for (process_t *tail = proc_head; tail->next != NULL; tail = tail->next)
    {
        if (tail->next == proc)
        {
            if (proc_head == proc)
            {
                proc_head = tail;
            }

            tail->next = proc->next;
            if (tail->next == tail)
            {
                tail->next = NULL;
            }
            return 0;
        }
    }

    if (proc_head == proc)
    {
        proc_head = proc->next;
    }

    if (!proc_head)
    {
        PANIC("no process running");
    }

    return -RES_INVARG; // not found
}

void task_execute(uint64_t rip, uint64_t rsp, uint64_t eflags, task_state_t *state);

int execute_next_process(void)
{
    if (!proc_head)
    {
        return -RES_CORRUPT;
    }

    if (!current_proc)
    {
        current_proc = proc_head;
    }
    else
    {
        current_proc = current_proc->next;
        if (!current_proc)
        {
            current_proc = proc_head;
        }
    }

    task_state_t state = current_proc->task.state; // needs to be copied because proc is allocated and not mapped in processes pml4

    int status = pml4_switch(current_proc->pml4);
    if (status < 0)
    {
        return status;
    }

    // TODO: execute global constructors
    task_execute(state.rip, state.rsp, 0x202, &state);

    return 0; // never executed
}

process_t *get_current_process(void)
{
    return current_proc;
}

process_t *get_process_from_pid(uint64_t pid)
{
    for (process_t *proc = proc_head; proc != NULL; proc = proc->next)
    {
        if (proc->pid == pid)
        {
            return proc;
        }
    }

    return NULL;
}
