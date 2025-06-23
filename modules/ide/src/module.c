#include <ide.h>
#include <kernel/module.h>
#include <kernel/log.h>
#include <kernel/dev/devm.h>

#define PCI_CLASS_MASS_STORAGE_CONTROLLER 0x01
#define PCI_SUBCLASS_IDE_CONTROLLER 0x01

driver_t ide_driver = {
    .device_type = DEVICE_TYPE_BLOCKDEV,
    .num_devices = 4,
    .create_bdev = &ide_create,

    .class_code = PCI_CLASS_MASS_STORAGE_CONTROLLER,
    .subclass_code = PCI_SUBCLASS_IDE_CONTROLLER,
    .prog_if = 0xFF,

    .driver_name = "IDE Controller",
    .device_name = "ATA, SATA or ATAPI Disk",
    .module = "ide",
    .author = "Nico Grundei"
};

KMOD_INIT(ide, Nico Grundei)
{
    if (IS_ERROR(register_driver(&ide_driver)))
    {
        PANIC("failed to register ide driver");
    }
}
