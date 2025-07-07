#include <virtio_entropy.h>
#include <kernel/dev/virtio.h>
#include <kernel/kmm.h>
#include <kernel/isr.h>

static virtio_device_t *virtio_dev = NULL;
static virtqueue_t *vq = NULL;

int virtio_entropy_free(device_t *dev)
{
    (void)dev;
    return 0;
}

int randomize_buffer(uint8_t *data, size_t size, device_t *dev)
{
    (void)dev;
    if (virtio_send_buffer(virtio_dev, vq, (uint64_t)data, size) < 0)
    {
        return -RES_EUNKNOWN;
    }
    return 0;
}

static void virtio_entropy_irq(interrupt_frame_t *frame)
{
    (void)frame;
    uint8_t isr_status = *(volatile uint8_t *)virtio_dev->isr;
    (void)isr_status;
}

extern driver_t virtio_entropy_driver;
device_ops_t virtio_entropy_ops = {
    .free = &virtio_entropy_free,
    .randomize_buffer = &randomize_buffer,
};

device_t *virtio_entropy_create(size_t index, pci_device_t *pci_dev)
{
    (void)index;

    if (virtio_dev != NULL)
    {
        PANIC("virtio gpu initialized twice");
    }

    device_t *device = kmalloc(sizeof(device_t));
    if (!device)
    {
        return NULL;
    }

    device->type = DEVICE_CHAR;
    device->driver = &virtio_entropy_driver;
    device->ops = &virtio_entropy_ops;
    device->pci_dev = pci_dev;

    pci_enable_device(pci_dev);

    register_interrupt_handler(42, &virtio_entropy_irq);

    virtio_dev = virtio_init(pci_dev);
    vq = virtio_setup_queue(virtio_dev, 0);
    virtio_start(virtio_dev);

    return device;
}
