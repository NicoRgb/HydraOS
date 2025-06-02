#include <kernel/log.h>
#include <kernel/dbg.h>
#include <kernel/kernel.h>

// STACK SMASH PROTECTOR

uint64_t __stack_chk_guard = 0x00; // gets randomized by bootstrap.asm

void __stack_chk_fail(void)
{
    PANIC_MINIMAL("detected stack smash");
}

// STACK TRACE

struct stackframe
{
    struct stackframe *rbp;
    uint64_t rip;
};

const char *resolve_function_name(uintptr_t addr, const Elf64_Sym *symbols, size_t num_symbols, const char *strtab)
{
    for (size_t i = 0; i < num_symbols; i++)
    {
        uintptr_t sym_start = symbols[i].st_value;
        uintptr_t sym_end = sym_start + symbols[i].st_size;

        if (addr >= sym_start && addr < sym_end)
        {
            return &strtab[symbols[i].st_name];
        }
    }

    return NULL;
}

void trace_stack(uint32_t max_frames)
{
    struct stackframe *stack;
    __asm__ volatile("movq %%rbp,%0" : "=r"(stack)::);

    for (uint32_t frame = 0; stack && frame < max_frames; frame++)
    {
        const char *symbol_name = NULL;
        if (boot_info.symtabndx != (uint32_t)-1 && boot_info.num_elf_sections > 0)
        {
            const Elf64_Shdr *symtab = &boot_info.elf_sections[boot_info.symtabndx];
            const char *strtab = (const char *)boot_info.elf_sections[symtab->sh_link].sh_addr;

            symbol_name = resolve_function_name(stack->rip, (const Elf64_Sym *)symtab->sh_addr, (symtab->sh_size / symtab->sh_entsize), strtab);
        }

        if (!symbol_name)
        {
            symbol_name = "unknown";
        }
        
        klog_raw(LOG_LEVEL_ERROR, " - addr: %p (%s)\n", stack->rip, symbol_name);
        stack = stack->rbp;
    }
}
