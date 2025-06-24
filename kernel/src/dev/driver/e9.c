#include <kernel/dev/devm.h>
#include <kernel/port.h>
#include <kernel/kmm.h>
#include <stdbool.h>

static bool e9_initialized = false;

int e9_write(char c, chardev_color_t fg, chardev_color_t bg, device_t *dev)
{
    if (!dev)
    {
        return -RES_INVARG;
    }

    (void)fg;
    (void)bg;

    port_byte_out(0xE9, c);

    return 0;
}

int e9_free(device_t *dev)
{
    if (!dev)
    {
        return -RES_INVARG;
    }

    kfree(dev);

    return 0;
}

device_t *e9_create(size_t index, struct _pci_device *pci_dev);

driver_t e9_driver = {
    .supported_type = DEVICE_CHAR,
    .num_devices = 1,
    .init_device = &e9_create,

    .class_code = 0xFF,
    .subclass_code = 0xFF,
    .prog_if = 0xFF,

    .driver_name = "0xE9 Port Hack",
    .device_name = "Serial Output",
    .module = "none",
    .author = "Nico Grundei"
};

device_ops_t e9_ops = {
    .free = &e9_free,
    .write = &e9_write,
};

device_t *e9_create(size_t index, struct _pci_device *pci_dev)
{
    (void)index;
    (void)pci_dev;

    if (e9_initialized)
    {
        return NULL;
    }

    device_t *dev = kmalloc(sizeof(device_t));
    if (!dev)
    {
        return NULL;
    }
    e9_initialized = true;

    dev->type = DEVICE_CHAR;
    dev->driver_data = NULL;
    dev->driver = &e9_driver;
    dev->ops = &e9_ops;
    dev->pci_dev = pci_dev;

    return dev;
}
