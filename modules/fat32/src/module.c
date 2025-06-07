#include <fat32.h>
#include <kernel/module.h>
#include <kernel/log.h>

filesystem_t fat32_filesystem = {
    .fs_init = &fat32_init,
    .fs_free = &fat32_free,
    .fs_test = &fat32_test,
    .fs_open = &fat32_open,
    .fs_close = &fat32_close,
    .fs_read = &fat32_read,
    .fs_write = &fat32_write,
    .fs_readdir = &fat32_readdir,
    .fs_delete = &fat32_delete
};

KMOD_INIT(fat32, Nico Grundei)
{
    if (IS_ERROR(register_filesystem(&fat32_filesystem)))
    {
        PANIC("failed to register fat32 filesystem");
    }
}
