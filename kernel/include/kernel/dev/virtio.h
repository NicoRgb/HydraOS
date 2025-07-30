#ifndef _KERNEL_VIRTIO_H
#define _KERNEL_VIRTIO_H

#include <stdint.h>
#include <kernel/status.h>
#include <kernel/dev/pci.h>

#define VIRTIO_STATUS_ACKNOWLEDGE 0x1
#define VIRTIO_STATUS_DRIVER 0x2
#define VIRTIO_STATUS_DRIVER_OK 0x4
#define VIRTIO_STATUS_FEATURES_OK 0x8
#define VIRTIO_STATUS_FAILED 0x80

#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG 3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
#define VIRTIO_PCI_CAP_PCI_CFG 5
#define VIRTIO_PCI_CAP_SHARED_MEMORY_CFG 8
#define VIRTIO_PCI_CAP_VENDOR_CFG 9

#define PCI_CAP_PTR_OFFSET 0x34
#define PCI_CAP_ID_VNDR 0x09

typedef struct __attribute__((packed))
{
    uint8_t cap_vndr;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t cfg_type;
    uint8_t bar;
    uint8_t padding[3];
    uint32_t offset;
    uint32_t length;
} virtio_pci_cap_t;

typedef struct __attribute__((packed))
{
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t config_msix_vector;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;

    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_driver;
    uint64_t queue_device;
    uint16_t queue_notif_config_data;
    uint16_t queue_reset;

    uint16_t admin_queue_index;
    uint16_t admin_queue_num;
} virtio_pci_common_cfg_t;

typedef struct __attribute__((packed))
{
    virtio_pci_cap_t cap;
    uint32_t notify_off_multiplier;
} virtio_pci_notify_cap_t;

typedef struct __attribute__((packed))
{
    virtio_pci_common_cfg_t *common;
    virtio_pci_notify_cap_t *notify;
    void *device_cfg;
    uint8_t *isr;
    uint32_t notify_off_multiplier;
} virtio_device_t;

struct virtq_desc
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail
{
    uint16_t flags;
    volatile uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct virtq_used_elem
{
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtq_used
{
    uint16_t flags;
    volatile uint16_t idx;
    struct virtq_used_elem ring[];
} __attribute__((packed));

#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2
#define VIRTQ_DESC_F_INDIRECT 4

typedef struct
{
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;

    uint16_t last_used_idx;
    uint16_t next_desc_idx;

    int queue_index;
    uint16_t queue_size;

    virtio_pci_common_cfg_t *common;
} virtqueue_t;

virtio_device_t *virtio_init(pci_device_t *pci_dev, uint32_t (*feature_negotiate)(uint32_t features, virtio_device_t *device, bool *abort));
KRES virtio_start(virtio_device_t *dev);
virtqueue_t *virtio_setup_queue(virtio_device_t *device, int queue_index);

KRES virtio_send_buffer(virtio_device_t *dev, virtqueue_t *vq, uint64_t buf, uint32_t len, uint8_t flags, bool wait);
KRES virtio_send(virtio_device_t *dev, virtqueue_t *vq, uint64_t cmd, uint32_t cmd_len, uint64_t resp, uint32_t resp_len);

#endif