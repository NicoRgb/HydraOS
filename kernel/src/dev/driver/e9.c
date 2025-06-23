#include <kernel/dev/chardev.h>
#include <kernel/dev/devm.h>
#include <kernel/port.h>
#include <kernel/kmm.h>
#include <stdbool.h>

static bool e9_initialized = false;

int e9_write(char c, chardev_color_t fg, chardev_color_t bg, chardev_t *cdev)
{
    if (!cdev)
    {
        return -RES_INVARG;
    }

    (void)fg;
    (void)bg;

    port_byte_out(0xE9, c);

    return 0;
}

int e9_free(chardev_t *cdev)
{
    if (!cdev)
    {
        return -RES_INVARG;
    }

    kfree(cdev);

    return 0;
}

chardev_t *e9_create(size_t index, struct _pci_device *pci_dev)
{
    (void)index;
    (void)pci_dev;

    if (e9_initialized)
    {
        return NULL;
    }

    chardev_t *cdev = kmalloc(sizeof(chardev_t));
    if (!cdev)
    {
        return NULL;
    }
    e9_initialized = true;

    cdev->write = &e9_write;
    cdev->free = &e9_free;
    cdev->references = 1;

    return cdev;
}

driver_t e9_driver = {
    .device_type = DEVICE_TYPE_CHARDEV,
    .num_devices = 1,
    .create_cdev = &e9_create,

    .class_code = 0xFF,
    .subclass_code = 0xFF,
    .prog_if = 0xFF,

    .driver_name = "0xE9 Port Hack",
    .device_name = "Serial Output",
    .module = "none",
    .author = "Nico Grundei"
};
