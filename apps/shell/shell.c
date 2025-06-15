#include <hydra/kernel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LINE_ALLOCATION_SIZE 256
#define TOKEN_BUFFER_SIZE 8
#define TOKEN_DELIM " \t\r\n\a"

int shell_help(char **args)
{
    fputs("Hydra Shell v1.0\n", stdout);
    return 0;
}

int shell_exit(char **args)
{
    syscall_exit(0);
}

char line[50];
char *shell_getline(char *line)
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

    return line;
}

char **split_line(char *line)
{
    int bufsize = TOKEN_BUFFER_SIZE;
    int pos = 0;

    char **tokens = malloc(bufsize * sizeof(char *));
    if (tokens == NULL)
    {
        fputs("allocation failure", stdout);
        syscall_exit(1);
    }

    char *token = strtok(line, TOKEN_DELIM);
    while (token != NULL)
    {
        tokens[pos++] = token;

        if (pos >= bufsize)
        {
            bufsize += TOKEN_BUFFER_SIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (tokens == NULL)
            {
                fputs("allocation failure", stdout);
                syscall_exit(1);
            }
        }

        token = strtok(NULL, TOKEN_DELIM);
    }

    tokens[pos] = NULL;
    return tokens;
}

int shell_launch(char **args)
{
    int64_t pid = syscall_fork();
    if (pid < 0)
    {
        fputs("failed to fork process\n", stdout);
        syscall_exit(1);
    }
    
    if (pid == 0)
    {
        uint16_t num_args;
        for (num_args = 0; args[num_args] != NULL; num_args++);

        printf("executing %s\n", args[0]);
        syscall_exec(args[0], num_args, (const char **)args);

        fputs("failed to execute process\n", stdout);
        syscall_exit(1);
    }

    while (syscall_ping(pid) == pid);

    return pid;
}

int shell_execute(char **args)
{
    if (args[0] == NULL)
    {
        return 0;
    }

    if (strcmp(args[0], "exit") == 0)
    {
        return shell_exit(args);
    }
    else if (strcmp(args[0], "help") == 0)
    {
        return shell_help(args);
    }

    return shell_launch(args);
}

void shell_loop(void)
{
    shell_getline(line);
    if (!line)
    {
        return;
    }

    char **args = split_line(line);
    if (!args)
    {
        return;
    }

    if (shell_execute(args) != 0)
    {
        return;
    }
}

int main(void)
{
    while (1)
    {
        printf("> ");
        shell_loop();
    }
    return 0;
}
