#include <hydra/kernel.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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

static bool is_full_path(const char *str)
{
    return str[0] == '/';
}

static bool file_exists(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}

static char *strdup(const char *str)
{
    char *dup = malloc(strlen(str) + 1);
    strcpy(dup, str);
    return dup;
}

int shell_launch(char **args)
{
    char *path = NULL;
    if (is_full_path(args[0]) && file_exists(args[0]))
    {
        path = strdup(args[0]);
    }
    else
    {
        const char *path_env = getenv("PATH");
        if (!path_env)
        {
            printf("PATH variable not set\n");
            return 1;
        }

        char *path_copy = strdup(path_env);
        if (!path_copy)
        {
            printf("allocation failure");
            return 1;
        }

        char *dir = strtok(path_copy, ":");
        while (dir != NULL)
        {
            if (!is_full_path(dir))
            {
                dir = strtok(NULL, ":");
                continue;
            }

            char *p = malloc(strlen(dir) + strlen(args[0]) + 2);
            strcpy(p, dir);
            p = strcat(p, "/");
            p = strcat(p, args[0]);
            
            if (file_exists(p))
            {
                path = p;
                break;
            }

            free(p);

            dir = strtok(NULL, ":");
        }

        free(path_copy);
    }

    if (!path)
    {
        printf("%s: command not found\n", args[0]);
        return 1;
    }

    int fd = syscall_pipe();
    if (fd < 0)
    {
        fputs("failed to pipe\n", stdout);
    }

    int64_t pid = syscall_fork();
    if (pid < 0)
    {
        fputs("failed to fork process\n", stdout);
        free(path);
        syscall_exit(1);
    }
    
    if (pid == 0)
    {
        uint16_t num_args;
        for (num_args = 0; args[num_args] != NULL; num_args++);

        process_create_info_t create_info = {
            .args = (const char **)args,
            .num_args = num_args,
            .envars = NULL,
            .num_envars = 0,
            .stdin_idx = (uint64_t)stdin,
            .stdout_idx = (uint64_t)fd,
            .stderr_idx = (uint64_t)stderr,
        };
        syscall_exec(path, &create_info);

        fputs("failed to execute process\n", stdout);
        free(path);
        syscall_exit(1);
    }

    free(path);

    char c = 0;
    do
    {
        c = fgetc((FILE *)fd);
        if (c == 0)
        {
            continue;
        }
        
        fputc(c, stdout);
    }
    while (syscall_ping(pid) == pid || c != 0);

    syscall_close((uint64_t)fd);

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
    printf("PATH='%s'\n", getenv("PATH"));

    while (1)
    {
        printf("> ");
        shell_loop();
    }
    return 0;
}
