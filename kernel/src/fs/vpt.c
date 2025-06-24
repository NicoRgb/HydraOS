#include <kernel/fs/vpt.h>
#include <kernel/kmm.h>

#define PARTITION_TABLES_CAPACITY_INCREASE 3

partition_table_t **partition_tables = NULL;
size_t partition_tables_capacity = 0;
size_t partition_tables_size = 0;

virtual_blockdev_t *vbdevs_head = NULL;

static int add_virtual_blockdev(virtual_blockdev_t *vbdev)
{
    if (!vbdev)
    {
        return -RES_INVARG;
    }

    if (!vbdevs_head)
    {
        vbdevs_head = vbdev;
        vbdev->prev = NULL;
        vbdev->next = NULL;
        return 0;
    }

    virtual_blockdev_t *vbdev_entry = NULL;
    for (vbdev_entry = vbdevs_head; vbdev_entry->next != NULL; vbdev_entry = vbdev_entry->next)
        ;

    if (!vbdev_entry)
    {
        return -RES_CORRUPT;
    }

    vbdev_entry->next = vbdev;
    vbdev->next = NULL;
    vbdev->prev = vbdev;
    
    return 0;
}

int scan_partition(device_t *bdev)
{
    partition_table_t *pt = NULL;
    for (size_t i = 0; i < partition_tables_size; i++)
    {
        if (partition_tables[i]->pt_test(bdev) == 0)
        {
            pt = partition_tables[i];
            break;
        }
    }

    if (!pt)
    {
        // raw device or unknown parition table
        virtual_blockdev_t *vbdev = kmalloc(sizeof(virtual_blockdev_t));
        if (!vbdev)
        {
            return -RES_NOMEM;
        }

        vbdev->pt = NULL;
        vbdev->bdev = bdev;
        vbdev->lba_offset = 0;
        vbdev->type = 0;
        vbdev->index = 0;

        int status = add_virtual_blockdev(vbdev);
        if (status < 0)
        {
            return status;
        }

        return 0;
    }

    void *pt_data = pt->pt_init(bdev);
    if (!pt_data)
    {
        return -RES_EUNKNOWN;
    }

    virtual_blockdev_t *vbdev = kmalloc(sizeof(virtual_blockdev_t));
    if (!vbdev)
    {
        return -RES_NOMEM;
    }

    vbdev->pt = pt;
    vbdev->bdev = bdev;
    for (uint8_t i = 0; pt->pt_get(i, pt_data, vbdev) >= 0 && i < UINT8_MAX; i++)
    {
        int status = add_virtual_blockdev(vbdev);
        if (status < 0)
        {
            return status;
        }

        vbdev = kmalloc(sizeof(virtual_blockdev_t));
        if (!vbdev)
        {
            return -RES_NOMEM;
        }
    }

    kfree(vbdev);

    int status = pt->pt_free(bdev, pt_data);
    if (status < 0)
    {
        return status;
    }

    return 0;
}

int free_virtual_blockdevs(device_t *bdev)
{
    if (!bdev)
    {
        return -RES_INVARG;
    }

    virtual_blockdev_t *vbdev = NULL;
    for (vbdev = vbdevs_head; vbdev != NULL; vbdev = vbdev->next)
    {
        if (vbdev->bdev != bdev)
        {
            continue;
        }

        vbdev->prev->next = vbdev->next;
        vbdev->next->prev = vbdev->prev;

        kfree(vbdev);
    }

    return 0;
}

virtual_blockdev_t *get_virtual_blockdev(device_t *bdev, uint8_t index)
{
    virtual_blockdev_t *vbdev = NULL;
    for (vbdev = vbdevs_head; vbdev != NULL; vbdev = vbdev->next)
    {
        if (vbdev->bdev == bdev && vbdev->index == index)
        {
            return vbdev;
        }
    }

    return NULL;
}

int register_partition_table(partition_table_t *pt)
{
    if (!pt)
    {
        return -RES_INVARG;
    }

    if (!partition_tables)
    {
        partition_tables = kmalloc(sizeof(partition_table_t *) * PARTITION_TABLES_CAPACITY_INCREASE);
        if (!partition_tables)
        {
            return -RES_NOMEM;
        }

        partition_tables_capacity = PARTITION_TABLES_CAPACITY_INCREASE;
        partition_tables_size = 0;
    }

    if (partition_tables_size >= partition_tables_capacity)
    {
        partition_tables = krealloc(partition_tables, partition_tables_capacity, partition_tables_capacity + sizeof(partition_table_t *) * PARTITION_TABLES_CAPACITY_INCREASE);
        partition_tables_capacity += PARTITION_TABLES_CAPACITY_INCREASE;
    }

    partition_tables[partition_tables_size++] = pt;

    return 0;
}
