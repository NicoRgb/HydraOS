#ifndef _KERNEL_TASK_H
#define _KERNEL_TASK_H

#include <stdint.h>
#include <stddef.h>

#include <kernel/status.h>
#include <kernel/vmm.h>
#include <kernel/proc/elf.h>
#include <kernel/proc/stream.h>

/*
 kernel:    0x100000
 heap:      0x200000
 process:   0x400000
 stack:     0x800000
*/

#define PROCESS_VADDR 0x400000

#define PROCESS_STACK_VADDR_BASE 0x800000
#define PROCESS_STACK_SIZE 4096 * 3

#define PROCESS_HEAP_VADDR_BASE 0x200000

#define PROCESS_MAX_STREAMS 8
#define PROCESS_MAX_HEAP_PAGES 32

typedef struct
{
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;
    uint64_t rip, rsp;
} __attribute__((packed)) task_state_t;

struct _process;

typedef struct _task
{
    void **stack_pages; // physical addresses
    size_t num_stack_pages;
    task_state_t state;
} task_t;

typedef struct _process
{
    task_t task;
    elf_file_t *elf;

    char path[MAX_PATH];
    page_table_t *pml4;
    void **data_pages; // physical addresses
    size_t num_data_pages;

    void *heap_pages[PROCESS_MAX_HEAP_PAGES];
    size_t num_heap_pages;

    stream_t *streams[PROCESS_MAX_STREAMS];

    char **arguments;
    uint16_t num_arguments;

    char **envars;
    uint16_t num_envars;

    uint64_t pid;
    
    struct _process *next;
} process_t;

void syscall_init(void);

process_t *process_create(const char *path);
void process_free(process_t *proc);
process_t *process_clone(process_t *proc);

int process_set_args(process_t *proc, char **args, uint16_t num_args);
int process_set_envars(process_t *proc, char **envars, uint16_t num_envars);

int process_set_stdin(process_t *proc, stream_t *stdin);
int process_set_stdout(process_t *proc, stream_t *stdout);
int process_set_stderr(process_t *proc, stream_t *stderr);

int setup_initial_stack(process_t *proc);
void *process_allocate_page(process_t *proc);
size_t process_insert_stream(process_t *proc, stream_t *stream);
size_t process_insert_file(process_t *proc, const char *path, uint8_t open_action);
void process_remove_stream(process_t *proc, size_t index);

int process_register(process_t *proc);
int process_unregister(process_t *proc);
int execute_next_process(void);
process_t *get_current_process(void);
process_t *get_process_from_pid(uint64_t pid);

#endif