#include <virtio_video.h>
#include <kernel/module.h>
#include <kernel/log.h>
#include <kernel/dev/devm.h>

#define PCI_CLASS_DISPLAY_CONTROLLER 0x03
#define PCI_SUBCLASS_DISPLAY_CONTROLLER_OTHER 0x80

driver_t virtio_video_driver = {
    .supported_type = DEVICE_CHAR,
    .num_devices = 1,
    .init_device = &virtio_video_create,

    .class_code = PCI_CLASS_DISPLAY_CONTROLLER,
    .subclass_code = PCI_SUBCLASS_DISPLAY_CONTROLLER_OTHER,
    .prog_if = 0xFF,

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
