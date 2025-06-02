#ifndef _KERNEL_DEVM_H
#define _KERNEL_DEVM_H

#include <kernel/dev/chardev.h>
#include <kernel/dev/blockdev.h>
#include <kernel/dev/inputdev.h>
#include <kernel/status.h>
#include <stddef.h>

typedef enum
{
    DEVICE_TYPE_CHARDEV = 0,
    DEVICE_TYPE_BLOCKDEV = 1,
    DEVICE_TYPE_INPUTDEV = 2
} device_handle_type_t;

typedef struct
{
    device_handle_type_t type;
    union
    {
        chardev_t *cdev;
        blockdev_t *bdev;
        inputdev_t *idev;
    };
} device_handle_t;

KRES init_devices(void);
void free_devices(void);

chardev_t *get_chardev(size_t index);
blockdev_t *get_blockdev(size_t index);
inputdev_t *get_inputdev(size_t index);

#endif