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

// Virtio PCI capability structure (common fields)
typedef struct __attribute__((packed)) {
    uint8_t cap_vndr;          // PCI capability ID (0x09)
    uint8_t cap_next;          // Next capability pointer
    uint8_t cap_len;           // Capability length
    uint8_t cfg_type;          // One of VIRTIO_PCI_CAP_*
    uint8_t bar;               // BAR index
    uint8_t padding[3];        // Reserved / padding
    uint32_t offset;           // Offset within BAR
    uint32_t length;           // Length of the capability
} virtio_pci_cap_t;

typedef struct __attribute__((packed))
{
    uint32_t device_feature_select; // Read-write
    uint32_t device_feature;        // Read-only
    uint32_t driver_feature_select; // Read-write
    uint32_t driver_feature;        // Read-write
    uint16_t config_msix_vector;    // Read-write
    uint16_t num_queues;            // Read-only
    uint8_t device_status;          // Read-write
    uint8_t config_generation;      // Read-only

    uint16_t queue_select;            // Read-write
    uint16_t queue_size;              // Read-write
    uint16_t queue_msix_vector;       // Read-write
    uint16_t queue_enable;            // Read-write
    uint16_t queue_notify_off;        // Read-only
    uint64_t queue_desc;              // Read-write
    uint64_t queue_driver;            // Read-write
    uint64_t queue_device;            // Read-write
    uint16_t queue_notif_config_data; // Read-only
    uint16_t queue_reset;             // Read-write

    uint16_t admin_queue_index; // Read-only
    uint16_t admin_queue_num;   // Read-only
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
    uint16_t idx;
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
    uint16_t idx;
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

virtio_device_t *virtio_init(pci_device_t *pci_dev);
KRES virtio_start(virtio_device_t *dev);
virtqueue_t *virtio_setup_queue(virtio_device_t *device, int queue_index);

KRES virtqueue_send_buffer(virtio_device_t *dev, virtqueue_t *vq, void *phys_buf, size_t len, bool write);
KRES virtio_send_buffer(virtio_device_t *dev, virtqueue_t *vq, uint64_t buf, uint32_t len);
KRES virtio_send(virtio_device_t *dev, virtqueue_t *vq, uint64_t cmd, uint32_t cmd_len, uint64_t resp, uint32_t resp_len);

#endif