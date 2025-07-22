#include <kernel/isr.h>
#include <kernel/smm.h>
#include <kernel/kprintf.h>
#include <kernel/port.h>
#include <kernel/proc/task.h>
#include <kernel/dbg.h>

#define INTERRUPT_GATE 0x8E
#define INTERRUPT_TRAP 0x8F

extern void *isr_stub_table[];
extern void *irq_stub_table[];

typedef struct
{
    uint16_t size;
    uint64_t offset;
} __attribute__((packed)) idt_ptr_t;

typedef struct
{
    uint16_t offset0;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset1;
    uint32_t offset2;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

void enable_interrupts(void)
{
    __asm__ volatile("sti");
}

void disable_interrupts(void)
{
    __asm__ volatile("cli");
}

__attribute((aligned(0x1000))) idt_entry_t idt[256];
__attribute((aligned(0x1000))) idt_ptr_t idt_ptr;

int set_idt_gate(uint32_t ino, void (*handler)(void))
{
    if (ino > 255)
    {
        return -RES_INVARG;
    }

    uint64_t offset = (uint64_t)handler;

    idt[ino].offset0 = offset & 0xFFFF;
    idt[ino].offset1 = (offset >> 16) & 0xFFFF;
    idt[ino].offset2 = (offset >> 32) & 0xFFFFFFFF;

    idt[ino].selector = KERNEL_CODE_SELECTOR;
    idt[ino].ist = 0x00;
    idt[ino].type_attributes = ino <= 31 ? INTERRUPT_TRAP : INTERRUPT_GATE;
    idt[ino].reserved = 0x00;

    return 0;
}

static inline void remap_pic()
{
    port_byte_out(0x20, 0x11);
    port_byte_out(0xA0, 0x11);
    port_byte_out(0x21, 0x20);
    port_byte_out(0xA1, 0x28);
    port_byte_out(0x21, 0x04);
    port_byte_out(0xA1, 0x02);
    port_byte_out(0x21, 0x01);
    port_byte_out(0xA1, 0x01);
    port_byte_out(0x21, 0x0);
    port_byte_out(0xA1, 0x0);
}

int interrupts_init(void)
{
    int res = 0;

    remap_pic();

    idt_ptr.size = (uint16_t)sizeof(idt) - 1;
    idt_ptr.offset = (uint64_t)idt;

    for (uint8_t i = 0; i < 32; i++)
    {
        set_idt_gate(i, isr_stub_table[i]);
    }
    for (uint8_t i = 32; i < 255; i++)
    {
        set_idt_gate(i, irq_stub_table[i - 32]);
    }

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));

    return res;
}

void (*interrupt_handlers[256])(interrupt_frame_t *frame);

int register_interrupt_handler(uint8_t irq, void (*handler)(interrupt_frame_t *))
{
    interrupt_handlers[irq] = handler;
    return 0;
}

extern page_table_t *kernel_pml4;

void irq_handler(interrupt_frame_t *frame)
{
    if (pml4_switch(kernel_pml4) < 0)
    {
        PANIC("failed to switch pml4");
    }

    if (frame->int_no >= 40)
    {
        port_byte_out(0xA0, 0x20);
    }
    port_byte_out(0x20, 0x20);
    
    if (interrupt_handlers[frame->int_no] != NULL)
    {
        interrupt_handlers[frame->int_no](frame);
    }

    process_t *proc = get_current_process();
    if (!proc)
    {
        return;
    }

    if (pml4_switch(proc->pml4) < 0)
    {
        PANIC("failed to switch pml4");
    }
}

char *exception_names[] = {
    "Division Error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
};

void exception_handler(interrupt_frame_t *frame)
{
    if (pml4_switch(kernel_pml4) < 0)
    {
        PANIC("failed to switch pml4");
    }

    LOG_ERROR("CPU exception triggered\n\n[Exception Info]\nType: %s\n", exception_names[frame->int_no]);
    switch (frame->int_no)
    {
    case 14: // Page Fault
        uint64_t cr2_val;
        __asm__ volatile("movq %%cr2, %0" : "=r"(cr2_val));

        LOG_ERROR("Error Code:");
        LOG_ERROR("- Tried to access virtual address 0x%x\n", cr2_val);
        if (frame->err_code & 0b1)
        {
            LOG_ERROR("- Couldn't complete because of page-protection violation");
        }
        else
        {
            LOG_ERROR("- Couldn't complete because page was not present");
        }
        if (frame->err_code & 0b10)
        {
            LOG_ERROR("- This was an attempt to WRITE to this address.");
        }
        else
        {
            LOG_ERROR("- This was an attempt to READ from this address.");
        }
        if (frame->err_code & 0b100)
        {
            LOG_ERROR("- Memory access came from user ('%s') at 0x%x.\n", get_current_process()->path, get_current_process()->task.state.rip);
        }
        else
        {
            LOG_ERROR("- Memory access came from kernel.");
        }
        if (frame->err_code & 0b1000)
        {
            LOG_ERROR("- caused by reading a 1 in a reserved field.");
        }
        if (frame->err_code & 0b10000)
        {
            LOG_ERROR("- caused by an instruction fetch.");
        }
        break;

    default:
        break;
    }

    LOG_ERROR("\n[Registers]\ncs=0x%x rip=0x%x\nrflags=0x%x error=0x%x\nrax=0x%x rcx=0x%x\nrdx=0x%x rsi=0x%x\nrdi=0x%x r8=0x%x\nr9=0x%x r10=0x%x\nr11=0x%x rbp=0x%x\nrsp=0x%x\n",
              frame->cs, frame->rip, frame->rflags, frame->err_code, frame->rax, frame->rcx, frame->rdx,
              frame->rsi, frame->rdi, frame->r8, frame->r9, frame->r10, frame->r11, frame->rsp);

    trace_stack(32, (void *)frame->rsp);

    __asm__ volatile("cli");
    __asm__ volatile("hlt");
    while (1);

    process_t *proc = get_current_process();
    if (!proc)
    {
        return;
    }

    if (pml4_switch(proc->pml4) < 0)
    {
        PANIC("failed to switch pml4");
    }
}
