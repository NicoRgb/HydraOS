#ifndef _KERNEL_MODULE_H
#define _KERNEL_MODULE_H

typedef struct
{
    void (*mod_init)(void);
    const char *mod_name;
    const char *mod_author;
} module_t;

int register_module(module_t *mod);

#define KMOD_INIT(name, author)                                      \
    void name##_module_init(void);                                   \
                                                                     \
    module_t name##_module = {                                       \
        .mod_init = &name##_module_init,                             \
        .mod_name = #name,                                           \
        .mod_author = #author};                                      \
                                                                     \
    __attribute__((constructor)) void name##_module_contructor(void) \
    {                                                                \
        register_module(&name##_module);                             \
    }                                                                \
                                                                     \
    void name##_module_init(void)

#endif