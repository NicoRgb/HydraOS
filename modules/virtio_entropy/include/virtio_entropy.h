#ifndef _VIRTIO_ENTROPY_VIRTIO_ENTROPY_H
#define _VIRTIO_ENTROPY_VIRTIO_ENTROPY_H

#include <kernel/dev/devm.h>
#include <kernel/dev/pci.h>

device_t *virtio_entropy_create(size_t index, pci_device_t *pci_dev);

#endif