#include <hydra/kernel.h>
#include <stdio.h>
#include <string.h>

uint8_t shell_get_line(char *line)
{
    line[0] = 0;

    uint8_t size = 0;
    while (size < 49)
    {
        char ascii = fgetc(stdin);
        if (ascii == 0)
        {
            continue;
        }

        fputc(ascii, stdout);

        if (ascii == '\n')
        {
            break;
        }

        line[size++] = ascii;
    }

    line[size] = 0;

    return size;
}

int64_t start_process(const char *path)
{
    int64_t pid = syscall_fork();
    if (pid < 0)
    {
        fputs("Failed to fork process\n", stdout);
        syscall_exit(1);
    }

    if (pid == 0)
    {
        syscall_exec(path);

        fputs("Failed to execute process\n", stdout);
        syscall_exit(1);
    }

    return pid;
}

int main(void)
{
    fputs("Hello Shell World!\n", stdout);

    char line[50];
    while (1)
    {
        uint8_t size = shell_get_line(line);
        if (strcmp(line, "exit") == 0)
        {
            syscall_exit(0);
        }

        int64_t pid = start_process(line);
        while (syscall_ping(pid) == pid);
    }

    return 0;
}
