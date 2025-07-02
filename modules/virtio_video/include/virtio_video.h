#ifndef _VIRTIO_VIDEO_VIRTIO_VIDEO_H
#define _VIRTIO_VIDEO_VIRTIO_VIDEO_H

#include <kernel/dev/devm.h>
#include <kernel/dev/pci.h>

device_t *virtio_video_create(size_t index, pci_device_t *pci_dev);

#endif