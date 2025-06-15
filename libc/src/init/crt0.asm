bits 64
section .text

extern main
;extern exit
extern initialize_standard_library

global _start
_start:
    mov rbp, 0
    push qword rbp
    push qword rbp
    mov qword rbp, rsp

    call initialize_standard_library

    call main

    ;mov edi, eax
    ;call exit

    mov edi, eax
    mov rax, 3
    syscall ; exit syscall
