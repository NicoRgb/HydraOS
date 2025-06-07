#include <kernel/module.h>
#include <kernel/kmm.h>

#define MODULES_CAPACITY_INCREASE 3

module_t *modules[8];
size_t num_modules = 0;

int register_module(module_t *mod)
{
    if (!mod)
    {
        return -RES_INVARG;
    }

    if (num_modules >= 8)
    {
        return -RES_NOMEM;
    }

    modules[num_modules++] = mod;

    LOG_INFO("initializing module %s by %s", mod->mod_name, mod->mod_author);
    mod->mod_init();

    return RES_SUCCESS;
}
