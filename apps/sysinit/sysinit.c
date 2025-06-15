#include <hydra/kernel.h>
#include <stdio.h>

int64_t start_shell(void)
{
    int64_t pid = syscall_fork();
    if (pid < 0)
    {
        fputs("SYSINIT -- ERROR UNRECOVERABLE -- FAILED TO FORK PROCESS\n", stdout);
        syscall_exit(1);
    }

    if (pid == 0)
    {
        syscall_exec("0:/bin/shell", 0, NULL);

        fputs("SYSINIT -- ERROR UNRECOVERABLE -- FAILED TO EXECUTE PROCESS\n", stdout);
        syscall_exit(1);
    }

    return pid;
}

int main(void)
{
    while (1)
    {
        int64_t pid = start_shell();
        while (syscall_ping(pid) == pid);

        fputs("SYSINIT -- INFO -- STARTING NEW SHELL\n", stdout);
    }

    return 1;
}
