#include <mbr.h>
#include <kernel/module.h>
#include <kernel/log.h>

partition_table_t mbr_partition_table = {
    .pt_init = &mbr_init,
    .pt_free = &mbr_free,
    .pt_test = &mbr_test,
    .pt_get = &mbr_get
};

KMOD_INIT(mbr, Nico Grundei)
{
    if (IS_ERROR(register_partition_table(&mbr_partition_table)))
    {
        PANIC("failed to register mbr partition table");
    }
}
