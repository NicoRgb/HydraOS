#include <virtio_net.h>
#include <kernel/module.h>
#include <kernel/log.h>
#include <kernel/dev/devm.h>

driver_t virtio_net_driver = {
    .supported_type = DEVICE_NET,
    .num_devices = 1,
    .init_device = &virtio_net_create,

    .class_code = 0xFF,
    .subclass_code = 0xFF,
    .prog_if = 0xFF,
    .vendor_id = 0x1AF4,
    .device_id = 0x1000,

    .driver_name = "Virtio based Network Controller",
    .device_name = "Virtual Network Card",
    .module = "virtio_net",
    .author = "Nico Grundei"
};

KMOD_INIT(virtio_net, Nico Grundei)
{
    if (IS_ERROR(register_driver(&virtio_net_driver)))
    {
        PANIC("failed to register virtio net driver");
    }
}
