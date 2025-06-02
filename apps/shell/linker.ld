OUTPUT_FORMAT(elf64-x86-64)

ENTRY(_start)

SECTIONS
{
    . = 0x400000;

    .text BLOCK(4K) : ALIGN(4K) {
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K) {
        *(.bss)
        *(COMMON)
    }

    .init BLOCK(4K) : ALIGN(4K) {
        *(.init)
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
        *(.note.gnu.build-id)
    }
}
