# Bug Writeup

## Security Vonurability in kernel syscall API

proccess_get_pointer is not limited in size and can overwrite ring0 memory

## Userland bug

userland heap memory is mapped into a continous region but proccess_get_pointer doesnt reflect this
