# Memory layout on rint0 and ring3

DISCLAIMER: Everything in this file descripes virtual memory

DISCLAIMER: The higher half kernel is not yet implemented

## User Memory Layout

0x0000_0000_0040_0000  ─┐
                        │  Process image (ELF text / data / bss)
0x0000_0000_0080_0000  ─┘

0x0000_0000_0080_0000  ─┐
                        │  User stack
                        │  (grows downward)
0x0000_0000_0100_0000  ─┘

0x0000_0000_0100_0000  ─┐
                        │  User heap
                        │  (grows upward)
                        │
                        └── dynamically mapped pages

0xffff_8000_0000_0000  ─┐
                        │  Kernel image
                        │  (.text / .rodata / .data / .bss)
                        │  Physically loaded at 1 MiB
0xffff_8000_0020_0000  ─┘

## Kernel Memory Layout

NOTE: The kernel could be identity mapped in an early stage

0xffff_8000_0000_0000  ─┐
                        │  Kernel image
                        │  (.text / .rodata / .data / .bss)
                        │  Physically loaded at 1 MiB
0xffff_8000_0020_0000  ─┘

0xffff_8000_0020_0000  ─┐
                        │  Kernel heap
                        │  (virtually contiguous)
                        │  Grows upward
                        │
0xffff_8000_0400_0000  ─┘

0xffff_8000_0400_0000  ─┐
                        │  Reserved virtual space
                        │  (future mappings, modules, etc.)
0xffff_8000_0800_0000  ─┘

0xffff_ffff_ffc0_0000  ─┐
                        │  Temporary kernel mappings
                        │  (e.g. mirroring user pages)
                        │
                        │
0xffff_ffff_ffff_ffff  ─┘
