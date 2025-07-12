#ifndef _VIRTIO_NET_VIRTIO_NET_H
#define _VIRTIO_NET_VIRTIO_NET_H

#include <kernel/dev/devm.h>
#include <kernel/dev/pci.h>

device_t *virtio_net_create(size_t index, pci_device_t *pci_dev);

#endif