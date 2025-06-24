#include <kernel/status.h>
#include <kernel/port.h>
#include <kernel/log.h>
#include <kernel/kmm.h>
#include <kernel/vmm.h>
#include <kernel/kernel.h>
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
    uint8_t id;
    uint8_t padding[2];
    uint32_t offset;
    uint32_t length;
} virtio_pci_cap_t;

// 64-bit virtio PCI capability structure
typedef struct __attribute__((packed))
{
    virtio_pci_cap_t cap; // Base capability structure
    uint32_t offset_hi;   // High 32 bits of offset
    uint32_t length_hi;   // High 32 bits of length
} virtio_pci_cap64_t;

// Common configuration structure
typedef struct __attribute__((packed))
{
    // About the whole device
    uint32_t device_feature_select; // Read-write
    uint32_t device_feature;        // Read-only
    uint32_t driver_feature_select; // Read-write
    uint32_t driver_feature;        // Read-write
    uint16_t config_msix_vector;    // Read-write
    uint16_t num_queues;            // Read-only
    uint8_t device_status;          // Read-write
    uint8_t config_generation;      // Read-only

    // About a specific virtqueue
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

    // About the administration virtqueue
    uint16_t admin_queue_index; // Read-only
    uint16_t admin_queue_num;   // Read-only
} virtio_pci_common_cfg_t;

// Notification capability structure
typedef struct __attribute__((packed))
{
    virtio_pci_cap_t cap;           // Base capability structure
    uint32_t notify_off_multiplier; // Multiplier for queue_notify_off
} virtio_pci_notify_cap_t;

typedef struct __attribute__((packed))
{
    virtio_pci_common_cfg_t *common;
    virtio_pci_notify_cap_t *notify;
} virtio_device_t;

struct virtq_desc {
    uint64_t addr;  // Physical address of the buffer
    uint32_t len;   // Buffer length
    uint16_t flags; // Descriptor flags
    uint16_t next;  // Index of the next descriptor
};

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[]; // descriptor index array
    // uint16_t used_event (if event idx enabled)
};

struct virtq_used_elem {
    uint32_t id;  // Index of descriptor
    uint32_t len; // Length written by device
};

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[];
    // uint16_t avail_event (if event idx enabled)
};

static uint8_t pci_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t data = pci_read(bus, device, function, offset);
    return (data >> ((offset & 3) * 8)) & 0xFF;
}

static void pci_read_bytes(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, void *buffer, uint8_t size)
{
    uint8_t *dst = (uint8_t *)buffer;
    for (uint8_t i = 0; i < size; ++i)
    {
        dst[i] = pci_read_byte(bus, device, function, offset + i);
    }
}

static void *virtio_map_bar(pci_device_t *dev, uint8_t bar, uint32_t offset)
{
    return (void *)(uintptr_t)(dev->bars[bar].address + offset);
}

virtio_device_t *device = NULL;

static void virtio_populate_device(pci_device_t *pci_dev)
{
    uint8_t cap_ptr = pci_read(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CAP_PTR_OFFSET);
    while (cap_ptr != 0)
    {
        virtio_pci_cap_t cap;
        pci_read_bytes(pci_dev->bus, pci_dev->device, pci_dev->function, cap_ptr, &cap, sizeof(cap));

        if (cap.cap_vndr == PCI_CAP_ID_VNDR)
        {
            switch (cap.cfg_type)
            {
            case VIRTIO_PCI_CAP_COMMON_CFG:
                device->common = virtio_map_bar(pci_dev, cap.bar, cap.offset);
                LOG_INFO("found common config");
                break;
            case VIRTIO_PCI_CAP_NOTIFY_CFG:
                device->notify = virtio_map_bar(pci_dev, cap.bar, cap.offset);
                LOG_INFO("found notify config");
                break;
            case VIRTIO_PCI_CAP_ISR_CFG:
                // virtio_map_bar(pci_dev, cap.bar, cap.offset);
                break;
            case VIRTIO_PCI_CAP_DEVICE_CFG:
                // virtio_map_bar(pci_dev, cap.bar, cap.offset);
                break;
            }
        }
        cap_ptr = cap.cap_next;
    }
}

static KRES virtio_setup_queue(int queue_index)
{
    device->common->queue_select = queue_index;

    uint16_t queue_size = device->common->queue_size;
    if (queue_size == 0)
    {
        return -RES_UNAVAILABLE;
    }

    void *page = pmm_alloc();
    slab_allocator_t slab;
    slab_init(&slab, page);

    memset(page, 0, PAGE_SIZE);

    size_t desc_size = sizeof(struct virtq_desc) * queue_size;
    size_t avail_size = sizeof(struct virtq_avail) + sizeof(uint16_t) * queue_size;
    size_t used_size = sizeof(struct virtq_used) + sizeof(struct virtq_used_elem) * queue_size;

    device->common->queue_desc = (uint64_t)slab_alloc(&slab, desc_size);
    if (device->common->queue_desc == 0)
    {
        return -RES_NOMEM;
    }

    device->common->queue_driver = (uint64_t)slab_alloc(&slab, avail_size);
    if (device->common->queue_driver == 0)
    {
        return -RES_NOMEM;
    }

    device->common->queue_device = (uint64_t)slab_alloc(&slab, used_size);
    if (device->common->queue_device == 0)
    {
        return -RES_NOMEM;
    }

    device->common->queue_enable = 1;

    return RES_SUCCESS;
}

extern page_table_t *kernel_pml4;

void virtio_init(pci_device_t *pci_dev)
{
    device = kmalloc(sizeof(virtio_device_t));
    memset(device, 0, sizeof(virtio_device_t));

    // obtain registers
    virtio_populate_device(pci_dev);

    if (!device->common || !device->notify)
    {
        PANIC("couldnt initialize virtio device: configs");
    }

    // map registers
    if (pml4_map(kernel_pml4, page_align_address_lower(device->common), page_align_address_lower(device->common), PAGE_PRESENT | PAGE_WRITABLE) != RES_SUCCESS)
    {
        PANIC("couldnt initialize virtio device: failed to map");
    }

    if (pml4_map(kernel_pml4, page_align_address_lower(device->notify), page_align_address_lower(device->notify), PAGE_PRESENT | PAGE_WRITABLE) != RES_SUCCESS)
    {
        PANIC("couldnt initialize virtio device: failed to map");
    }

    // reset device
    device->common->device_status = 0;

    while (device->common->device_status != 0)
        ; // TODO: set timeout

    device->common->device_status |= VIRTIO_STATUS_ACKNOWLEDGE;
    device->common->device_status |= VIRTIO_STATUS_DRIVER;

    device->common->device_feature_select = 0;
    uint32_t features = device->common->device_feature;

    device->common->driver_feature_select = 0;
    device->common->driver_feature = features;

    device->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    if (!(device->common->device_status & VIRTIO_STATUS_FEATURES_OK))
    {
        PANIC("couldnt initialize virtio device: features");
    }

    if (virtio_setup_queue(0) < 0)
    {
        PANIC("couldnt initialize virtio device: queue setup failed");
    }

    device->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
}

void virtio_free(void)
{
    if (device)
    {
        kfree(device);
    }
}
