bits 32
section .text

%define STACK_SIZE (4096 * 4)

global _start
extern _init
extern early_init
extern kmain

_start:
    jmp multiboot_entry

section .multiboot

; multiboot header

%define MB_MAGIC 0x1BADB002
%define MB_FLAGS 0b000011 ; page align modules, provide memory information

multiboot_header:
    dd MB_MAGIC
    dd MB_FLAGS
    dd -(MB_MAGIC + MB_FLAGS)

; multiboot2 header

%define MB2_MAGIC 0xE85250D6
%define MB2_ARCH_I386 0

align 8
multiboot2_header:
    dd MB2_MAGIC
    dd MB2_ARCH_I386
    dd .end - multiboot2_header
    dd -(MB2_MAGIC + MB2_ARCH_I386 + (.end - multiboot2_header))

align 8
.align_tag_start:
    dw 6 ; type
    dw 0
    dd .align_tag_end - .align_tag_start
.align_tag_end:

align 8
    ; multiboot2 header tag end
    dw 0
    dw 0
    dd 8

.end:

section .text
multiboot_entry:
    mov esp, stack + STACK_SIZE

    push 0
    popfd

    mov [multiboot_signature], eax
    mov [multiboot_structure], ebx

    call check_multiboot2
    call check_cpuid
    call check_long_mode
    
    call check_sse
    call enable_sse

    call setup_page_tables
    call switch_long_mode

    lgdt [gdt.ptr]

    mov ax, gdt.data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp gdt.code:long_mode_start

halt:
    cli
    hlt

%define VIDEO_MEMORY 0xb8000
%define WHITE_ON_BLACK 0x0f

print_msg:
    pusha
    mov edx, VIDEO_MEMORY
.loop:
    mov al, [ebx]
    mov ah, WHITE_ON_BLACK

    cmp al, 0
    je .done

    mov [edx], ax
    inc ebx
    add edx, 2

    jmp .loop
.done:
    popa
    ret

check_multiboot2:
    cmp eax, 0x36d76289
    jne check_multiboot
    ret

check_multiboot:
    cmp eax, 0x2BADB002
    jne .error
    ret
.error:
    mov ebx, mb_err_msg
    call print_msg
    jmp halt

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd

    pop eax
    push ecx
    popfd

    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov ebx, cpuid_err_msg
    call print_msg
    jmp halt

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode

    ret
.no_long_mode:
    mov ebx, long_mode_err_msg
    call print_msg
    jmp halt

check_sse:
    mov eax, 0x01
    cpuid
    test edx, 1<<25
    jz .no_sse

    ret
.no_sse:
    mov ebx, sse_err_msg
    call print_msg
    jmp halt

enable_sse:
    mov eax, cr0
    and ax, 0xFFFB
    or ax, 0x02
    mov cr0, eax
    mov eax, cr4
    or ax, 3<<9
    mov cr4, eax

    ret

setup_page_tables:
    mov eax, page_table_l3
    or eax, 0b11
    mov [page_table_l4], eax
    
    mov eax, page_table_l2
    or eax, 0b11
    mov [page_table_l3], eax
    
    xor ecx, ecx

.loop:
    mov eax, 0x200000
    imul eax, ecx
    or eax, 0b10000011
    mov [page_table_l2 + ecx*8], eax
    
    inc ecx
    cmp ecx, 512
    jne .loop

    ret

switch_long_mode:
    mov eax, page_table_l4
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

bits 64

long_mode_start:
    cli

    mov rsp, stack + STACK_SIZE
    mov rbp, rsp

    call init_ssp

    xor rbp, rbp ; important for stack tracing
    mov rdi, [multiboot_signature]
    mov rsi, [multiboot_structure]
    call early_init

    call _init ; global constructors

    xor rbp, rbp
    call kmain
    
    cli
    hlt

extern __stack_chk_guard
init_ssp:
    ; pseudo random number in rax
    rdtsc
    shl rdx, 32
    or rax, rdx

    mov qword [__stack_chk_guard], rax
    ret

section .data
multiboot_signature: dd 0
multiboot_structure: dq 0

section .rodata
PRESENT        equ 1 << 7
NOT_SYS        equ 1 << 4
EXEC           equ 1 << 3
DC             equ 1 << 2
RW             equ 1 << 1
ACCESSED       equ 1 << 0
 
GRAN_4K       equ 1 << 7
SZ_32         equ 1 << 6
LONG_MODE     equ 1 << 5

gdt:
    .null: equ $ - gdt
        dq 0
    .code: equ $ - gdt
        dd 0xFFFF                                   ; Limit & Base (low, bits 0-15)
        db 0                                        ; Base (mid, bits 16-23)
        db PRESENT | NOT_SYS | EXEC | RW            ; Access
        db GRAN_4K | LONG_MODE | 0xF                ; Flags & Limit (high, bits 16-19)
        db 0                                        ; Base (high, bits 24-31)
    .data: equ $ - gdt
        dd 0xFFFF                                   ; Limit & Base (low, bits 0-15)
        db 0                                        ; Base (mid, bits 16-23)
        db PRESENT | NOT_SYS | RW                   ; Access
        db GRAN_4K | SZ_32 | 0xF                    ; Flags & Limit (high, bits 16-19)
        db 0                                        ; Base (high, bits 24-31)
    .ptr:
        dw $ - gdt - 1
        dq gdt

mb_err_msg: db "error: boot failed... halting", 0
cpuid_err_msg: db "error: no cpuid... halting", 0
long_mode_err_msg: db "error: no long mode... halting", 0
sse_err_msg: db "error: no sse... halting", 0

section .bss
align 4096
page_table_l4 resb 4096

align 4096
page_table_l3 resb 4096

align 4096
page_table_l2 resb 4096

align 4096
stack resb STACK_SIZE
