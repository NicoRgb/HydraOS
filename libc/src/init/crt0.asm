bits 64
section .text

extern main
extern initialize_standard_library

extern _argc
extern _argv

global _start
_start:
    mov rbp, 0
    push qword rbp
    push qword rbp
    mov qword rbp, rsp

    push qword rdi
    push qword rsi

    call initialize_standard_library

    pop qword rsi
    pop qword rdi

    call main

    mov edi, eax
    mov rax, 3
    syscall ; exit syscall
