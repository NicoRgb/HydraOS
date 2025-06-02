#include <kernel/proc/scheduler.h>
#include <kernel/proc/task.h>
#include <kernel/pit.h>
#include <kernel/kprintf.h>

static void scheduler_handler(interrupt_frame_t *frame, uint32_t)
{
    process_t *proc = get_current_process();
    if (!proc)
    {
        return;
    }

    // save state
    proc->task->state.r15 = frame->r15;
    proc->task->state.r14 = frame->r14;
    proc->task->state.r13 = frame->r13;
    proc->task->state.r12 = frame->r12;
    proc->task->state.r11 = frame->r11;
    proc->task->state.r10 = frame->r10;
    proc->task->state.r9 = frame->r9;
    proc->task->state.r8 = frame->r8;
    proc->task->state.rsi = frame->rsi;
    proc->task->state.rdi = frame->rdi;
    proc->task->state.rbp = frame->rbp;
    proc->task->state.rdx = frame->rdx;
    proc->task->state.rcx = frame->rcx;
    proc->task->state.rbx = frame->rbx;
    proc->task->state.rax = frame->rax;
    proc->task->state.rip = frame->rip;
    proc->task->state.rsp = frame->rsp;

    execute_next_process();
    PANIC("failed to execute process");
}

void scheduler_init(void)
{
    return register_pit_handler(&scheduler_handler);
}
