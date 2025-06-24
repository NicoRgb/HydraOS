#ifndef _KERNEL_VPT_H
#define _KERNEL_VPT_H

#include <stdint.h>
#include <stddef.h>

#include <kernel/status.h>
#include <kernel/dev/devm.h>

struct _partition_table;

typedef struct _virtual_blockdev
{
    device_t *bdev;
    size_t lba_offset;
    uint32_t type;
    uint8_t index;
    struct _partition_table *pt;

    struct _virtual_blockdev *prev;
    struct _virtual_blockdev *next;
} virtual_blockdev_t;

int scan_partition(device_t *bdev);
int free_virtual_blockdevs(device_t *bdev);
virtual_blockdev_t *get_virtual_blockdev(device_t *bdev, uint8_t index);

typedef struct _partition_table
{
    void *(*pt_init)(device_t *);
    int (*pt_free)(device_t *, void *);
    int (*pt_test)(device_t *);

    int (*pt_get)(uint8_t, void *, virtual_blockdev_t *);
} partition_table_t;

int register_partition_table(partition_table_t *pt);

#endif