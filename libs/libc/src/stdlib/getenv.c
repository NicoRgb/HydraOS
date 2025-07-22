#include <stdlib.h>
#include <string.h>
#include <vec.h>

extern cvector(char *) environment_args;

char *getenv(const char *name)
{
    size_t name_len = strlen(name);

    for (size_t i = 0; i < cvector_size(environment_args); i++)
    {
        char *entry = environment_args[i];

        if (strncmp(entry, name, name_len) == 0 && entry[name_len] == '=')
        {
            return entry + name_len + 1;
        }
    }

    return NULL; // Not found
}
