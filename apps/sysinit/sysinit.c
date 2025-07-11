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
        const char *envp = "PATH=0:/bin";

        process_create_info_t create_info = {
            .args = NULL,
            .num_args = 0,
            .envars = &envp,
            .num_envars = 1,
            .stdin_idx = (uint64_t)stdin,
            .stdout_idx = (uint64_t)stdout,
            .stderr_idx = (uint64_t)stderr,
        };
        syscall_exec("0:/bin/shell", &create_info);

        fputs("SYSINIT -- ERROR UNRECOVERABLE -- FAILED TO EXECUTE PROCESS\n", stdout);
        syscall_exit(1);
    }

    return pid;
}

int main(void)
{
    _stdin = (uint64_t)fopen("9:/input0", "r");
    _stdout = (uint64_t)fopen("9:/char1", "r");
    _stderr = (uint64_t)fopen("9:/char1", "r");

    while (1)
    {
        int64_t pid = start_shell();
        while (syscall_ping(pid) == pid);

        fputs("SYSINIT -- INFO -- STARTING NEW SHELL\n", stdout);
    }

    return 1;
}
