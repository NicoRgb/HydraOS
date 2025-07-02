#include <virtio_entropy.h>
#include <kernel/module.h>
#include <kernel/log.h>
#include <kernel/dev/devm.h>

driver_t virtio_video_driver = {
    .supported_type = DEVICE_RNG,
    .num_devices = 1,
    .init_device = &virtio_entropy_create,

    .class_code = 0xFF,
    .subclass_code = 0xFF,
    .prog_if = 0xFF,
    .vendor_id = ,
    .device_id = ,

    .driver_name = "Virtio based Entropy Harvester",
    .device_name = "Virtual RNG",
    .module = "virtio_entropy",
    .author = "Nico Grundei"
};

KMOD_INIT(virtio_entropy, Nico Grundei)
{
    if (IS_ERROR(register_driver(&virtio_entropy_driver)))
    {
        PANIC("failed to register virtio entropy driver");
    }
}
