OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS {
    . = 1M;
    __kernel_start = .;

    .multiboot : ALIGN(CONSTANT(MAXPAGESIZE)) {
        KEEP(*(.multiboot))
    }

    .text : ALIGN(CONSTANT(MAXPAGESIZE)) {
        *(.text)
    }

    .rodata : ALIGN(CONSTANT(MAXPAGESIZE)) {
        *(.rodata)
    }

    .data : ALIGN(CONSTANT(MAXPAGESIZE)) {
        *(.data)
    }

    .bss : ALIGN(CONSTANT(MAXPAGESIZE)) {
        *(.bss)
    }

    /DISCARD/ : {
        *(.comment)
    }

    __kernel_end = .;
}
