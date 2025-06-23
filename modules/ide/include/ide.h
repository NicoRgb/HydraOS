#ifndef _FAT32_FAT32_H
#define _FAT32_FAT32_H

#include <kernel/dev/blockdev.h>
#include <kernel/dev/pci.h>

blockdev_t *ide_create(size_t index, pci_device_t *pci_dev);

#endif