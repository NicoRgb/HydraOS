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

struct _pci_device;

typedef struct
{
    device_handle_type_t device_type;
    uint8_t num_devices; // one driver can create multiple device, for example ide controller
    union
    {
        blockdev_t *(*create_bdev)(size_t index, struct _pci_device *pci_dev);
        chardev_t *(*create_cdev)(size_t index, struct _pci_device *pci_dev);
        inputdev_t *(*create_idev)(size_t index, struct _pci_device *pci_dev);
    };

    // identification
    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t prog_if;

    // logging/debugging
    const char *driver_name;
    const char *device_name;
    const char *module;
    const char *author;
} driver_t;

KRES register_driver(driver_t *driver);

KRES init_devices(void);

chardev_t *get_chardev(size_t index);
blockdev_t *get_blockdev(size_t index);
inputdev_t *get_inputdev(size_t index);

#endif