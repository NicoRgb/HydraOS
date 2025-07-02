#include <virtio_video.h>
#include <kernel/module.h>
#include <kernel/log.h>
#include <kernel/dev/devm.h>

driver_t virtio_video_driver = {
    .supported_type = DEVICE_VIDEO,
    .num_devices = 1,
    .init_device = &virtio_video_create,

    .class_code = 0xFF,
    .subclass_code = 0xFF,
    .prog_if = 0xFF,
    .vendor_id = 0x1AF4,
    .device_id = 0x1050,

    .driver_name = "Virtio based Video Controller",
    .device_name = "Virtual GPU",
    .module = "virtio_video",
    .author = "Nico Grundei"
};

KMOD_INIT(virtio_video, Nico Grundei)
{
    if (IS_ERROR(register_driver(&virtio_video_driver)))
    {
        PANIC("failed to register virtio video driver");
    }
}
