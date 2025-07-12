#include <kernel/status.h>
#include <kernel/port.h>
#include <kernel/log.h>
#include <kernel/kmm.h>
#include <kernel/vmm.h>
#include <kernel/kernel.h>
#include <kernel/dev/virtio.h>

extern page_table_t *kernel_pml4;

static void *virtio_map_bar(pci_device_t *dev, uint8_t bar, uint32_t offset)
{
    uintptr_t phys_addr = dev->bars[bar].address + offset;
    uintptr_t phys_page = phys_addr & ~(PAGE_SIZE - 1);

    void *virt_addr = (void *)phys_page;

    if (pml4_map(kernel_pml4, virt_addr, (void *)phys_page, PAGE_PRESENT | PAGE_WRITABLE) != RES_SUCCESS)
    {
        PANIC("Failed to map BAR physical page");
    }

    return (void *)phys_addr;
}

static uint8_t pci_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t data = pci_read(bus, device, function, offset & ~3);
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

static void virtio_populate_device(virtio_device_t *device, pci_device_t *pci_dev)
{
    uint8_t cap_ptr = pci_read_byte(pci_dev->bus, pci_dev->device, pci_dev->function, PCI_CAP_PTR_OFFSET);

    while (cap_ptr != 0)
    {
        virtio_pci_cap_t cap;
        pci_read_bytes(pci_dev->bus, pci_dev->device, pci_dev->function, cap_ptr, &cap, sizeof(virtio_pci_cap_t));

        if (cap.cap_vndr == PCI_CAP_ID_VNDR) // vendor-specific
        {
            switch (cap.cfg_type)
            {
            case VIRTIO_PCI_CAP_COMMON_CFG:
                device->common = virtio_map_bar(pci_dev, cap.bar, cap.offset);
                LOG_INFO("Found common config");
                break;

            case VIRTIO_PCI_CAP_NOTIFY_CFG:
            {
                virtio_pci_notify_cap_t notify_cap;
                pci_read_bytes(pci_dev->bus, pci_dev->device, pci_dev->function, cap_ptr, &notify_cap, sizeof(virtio_pci_notify_cap_t));

                device->notify = virtio_map_bar(pci_dev, notify_cap.cap.bar, notify_cap.cap.offset);
                device->notify_off_multiplier = notify_cap.notify_off_multiplier;
                LOG_INFO("Found notify config, multiplier=%u", device->notify_off_multiplier);
                break;
            }

            case VIRTIO_PCI_CAP_ISR_CFG:
                device->isr = virtio_map_bar(pci_dev, cap.bar, cap.offset);
                LOG_INFO("Found ISR config");
                break;

            case VIRTIO_PCI_CAP_DEVICE_CFG:
                device->device_cfg = virtio_map_bar(pci_dev, cap.bar, cap.offset);
                LOG_INFO("Found device config");
                break;

            default:
                LOG_INFO("Unknown virtio PCI capability type %u", cap.cfg_type);
                break;
            }
        }

        cap_ptr = cap.cap_next;
    }
}

virtqueue_t *virtio_setup_queue(virtio_device_t *device, int queue_index)
{
    device->common->queue_select = queue_index;

    uint16_t queue_size = device->common->queue_size;
    if (queue_size == 0)
    {
        LOG_ERROR("Queue size is zero");
        return NULL;
    }

    size_t desc_size = sizeof(struct virtq_desc) * queue_size;
    size_t avail_size = sizeof(struct virtq_avail) + sizeof(uint16_t) * queue_size;
    size_t used_size = sizeof(struct virtq_used) + sizeof(struct virtq_used_elem) * queue_size;

    // TODO: check if sizes fit

    void* desc = pmm_alloc();
    void* avail = pmm_alloc();
    void* used = pmm_alloc();

    if (!desc || !avail || !used) {
        LOG_ERROR("Failed to allocate virtqueue pages");
        return NULL;
    }

    device->common->queue_desc = (uint64_t)desc;
    device->common->queue_driver = (uint64_t)avail;
    device->common->queue_device = (uint64_t)used;

    virtqueue_t *vq = kmalloc(sizeof(virtqueue_t));
    if (!vq)
    {
        LOG_ERROR("Failed to allocate virtqueue struct");
        return NULL;
    }
    memset(vq, 0, sizeof(virtqueue_t));

    vq->desc = desc;
    vq->avail = avail;
    vq->used = used;

    memset(vq->desc,  0x00, PAGE_SIZE);
    memset(vq->avail, 0x00, PAGE_SIZE);
    memset(vq->used,  0x00, PAGE_SIZE);

    vq->last_used_idx = vq->used->idx;
    vq->next_desc_idx = 0;

    vq->queue_index = queue_index;
    vq->queue_size = queue_size;
    vq->common = device->common;

    device->common->queue_enable = 1;

    return vq;
}

virtio_device_t *virtio_init(pci_device_t *pci_dev, uint32_t (*feature_negotiate)(uint32_t features, virtio_device_t *device, bool *abort))
{
    virtio_device_t *device = kmalloc(sizeof(virtio_device_t));
    if (!device)
    {
        LOG_ERROR("Failed to allocate virtio_device");
        return NULL;
    }

    memset(device, 0, sizeof(virtio_device_t));

    // obtain registers and map them inside virtio_populate_device
    virtio_populate_device(device, pci_dev);

    if (!device->common || !device->notify)
    {
        PANIC("Couldn't initialize virtio device: missing common or notify config");
    }

    // reset device
    device->common->device_status = 0;

    while (device->common->device_status != 0); // TODO: add timeout here

    device->common->device_status |= VIRTIO_STATUS_ACKNOWLEDGE;
    device->common->device_status |= VIRTIO_STATUS_DRIVER;

    bool abort = false;

    device->common->device_feature_select = 0;
    uint32_t features = feature_negotiate(device->common->device_feature, device, &abort);
    if (abort)
    {
        kfree(device);
        return NULL;
    }

    device->common->driver_feature_select = 0;
    device->common->driver_feature = features;

    device->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    if (!(device->common->device_status & VIRTIO_STATUS_FEATURES_OK))
    {
        PANIC("Couldn't initialize virtio device: features not accepted");
    }

    return device;
}

KRES virtio_start(virtio_device_t *dev)
{
    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    return RES_SUCCESS;
}

KRES virtio_send_buffer(virtio_device_t *dev, virtqueue_t *vq, uint64_t buf, uint32_t len)
{
    if (len == 0)
    {
        return -RES_INVARG;
    }

    dev->common->queue_select = vq->queue_index;
    dev->common->queue_enable = 1;

    uint16_t size = vq->queue_size;
    uint16_t idx = vq->next_desc_idx;

    memset(&vq->desc[idx], 0, sizeof(struct virtq_desc));

    vq->desc[idx].addr = buf;
    vq->desc[idx].len = len;
    vq->desc[idx].flags = VIRTQ_DESC_F_NEXT;
    vq->desc[idx].next = (idx + 1) % size;

    vq->avail->ring[vq->avail->idx % size] = idx;
    vq->avail->idx++;

    vq->last_used_idx = vq->used->idx;
    vq->next_desc_idx = (idx + 1) % size;

    __sync_synchronize();

    *(volatile uint16_t *)(uintptr_t)(dev->notify + dev->notify_off_multiplier * vq->queue_index) = vq->queue_index;

    size_t timeout = 0xFFFFFFFFFFFFFFFF;
    while (vq->last_used_idx == vq->used->idx && timeout-- > 0);

    if (timeout <= 0)
    {
        LOG_ERROR("virtio_send timeout");
        return -RES_TIMEOUT;
    }

    return RES_SUCCESS;
}

KRES virtio_send(virtio_device_t *dev, virtqueue_t *vq, uint64_t cmd, uint32_t cmd_len, uint64_t resp, uint32_t resp_len)
{
    if (cmd_len == 0 || resp_len == 0)
    {
        return -RES_INVARG;
    }

    dev->common->queue_select = vq->queue_index;
    dev->common->queue_enable = 1;

    uint16_t size = vq->queue_size;
    uint16_t idx = vq->next_desc_idx;
    uint16_t cmd_idx = idx;

    memset(&vq->desc[idx], 0, sizeof(struct virtq_desc));

    vq->desc[idx].addr = cmd;
    vq->desc[idx].len = cmd_len;
    vq->desc[idx].flags = VIRTQ_DESC_F_NEXT;
    vq->desc[idx].next = (idx + 1) % size;

    idx = (idx + 1) % size;

    memset(&vq->desc[idx], 0, sizeof(struct virtq_desc));
    vq->desc[idx].addr = resp;
    vq->desc[idx].len = resp_len;
    vq->desc[idx].flags = VIRTQ_DESC_F_WRITE;

    vq->avail->ring[vq->avail->idx % size] = cmd_idx;
    vq->avail->idx++;

    vq->last_used_idx = vq->used->idx;
    vq->next_desc_idx = (idx + 1) % size;

    __sync_synchronize();

    *(volatile uint16_t *)(uintptr_t)(dev->notify + dev->notify_off_multiplier * vq->queue_index) = vq->queue_index;

    size_t timeout = 0xFFFFFFFFFFFFFFFF;
    while (vq->last_used_idx == vq->used->idx && timeout-- > 0);

    if (timeout <= 0)
    {
        LOG_ERROR("virtio_send timeout");
        return -RES_TIMEOUT;
    }

    return RES_SUCCESS;
}
