#include <virtio_video.h>
#include <kernel/dev/virtio.h>
#include <kernel/kmm.h>
#include <kernel/isr.h>

typedef struct
{
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t ring_idx;
    uint8_t padding[3];
} __attribute__((packed)) virtio_gpu_ctrl_hdr_t;

typedef struct
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) virtio_gpu_rect_t;

typedef struct
{
    virtio_gpu_ctrl_hdr_t hdr;
    struct virtio_gpu_display_one
    {
        virtio_gpu_rect_t rect;
        uint32_t enabled;
        uint32_t flags;
    } pmodes[16];
} __attribute__((packed)) virtio_gpu_resp_display_info_t;

typedef struct
{
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t rect;
    uint32_t scanout_id;
    uint32_t resource_id;
} __attribute__((packed)) virtio_gpu_set_scanout_t;

typedef enum
{
    /* 2d commands */
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    VIRTIO_GPU_CMD_RESOURCE_UNREF,
    VIRTIO_GPU_CMD_SET_SCANOUT,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO,
    VIRTIO_GPU_CMD_GET_CAPSET,
    VIRTIO_GPU_CMD_GET_EDID,
    VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB,
    VIRTIO_GPU_CMD_SET_SCANOUT_BLOB,

    /* 3d commands */
    VIRTIO_GPU_CMD_CTX_CREATE = 0x0200,
    VIRTIO_GPU_CMD_CTX_DESTROY,
    VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE,
    VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_3D,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D,
    VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D,
    VIRTIO_GPU_CMD_SUBMIT_3D,
    VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB,
    VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB,

    /* cursor commands */
    VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
    VIRTIO_GPU_CMD_MOVE_CURSOR,

    /* success responses */
    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET,
    VIRTIO_GPU_RESP_OK_EDID,
    VIRTIO_GPU_RESP_OK_RESOURCE_UUID,
    VIRTIO_GPU_RESP_OK_MAP_INFO,

    /* error responses */
    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
} __attribute__((packed)) control_type_t;

typedef enum
{
    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1,
    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM = 2,
    VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM = 3,
    VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM = 4,

    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM = 67,
    VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM = 68,

    VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM = 121,
    VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM = 134,
} __attribute__((packed)) virtio_gpu_formats_t;

typedef struct
{
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) virtio_gpu_resource_create_2d_t;

typedef struct
{
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_mem_entry_t;

typedef struct
{
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    virtio_gpu_mem_entry_t entries[]
} __attribute__((packed)) virtio_gpu_resource_attach_backing_t;

typedef struct
{
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t rect;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_transfer_to_host_2d_t;

typedef struct
{
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t rect;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_resource_flush_t;

typedef struct
{
    uint32_t id;
    uint32_t width;
    uint32_t height;
} virtio_resource_2d_t;

virtio_device_t *virtio_dev = NULL;
virtio_gpu_resp_display_info_t display_info = {};

int virtio_video_free(device_t *dev)
{
    (void)dev;
    return 0;
}

int virtio_video_get_display_rect(video_rect_t *rect, device_t *dev)
{
}

static int get_display_info(virtqueue_t *vq, virtio_gpu_resp_display_info_t *out)
{
    virtio_gpu_ctrl_hdr_t *cmd = (virtio_gpu_ctrl_hdr_t *)pmm_alloc();
    memset(cmd, 0, sizeof(virtio_gpu_ctrl_hdr_t));
    cmd->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    virtio_gpu_resp_display_info_t *resp = (virtio_gpu_resp_display_info_t *)pmm_alloc();

    virtio_send(virtio_dev, vq, (uintptr_t)cmd, sizeof(virtio_gpu_ctrl_hdr_t), (uintptr_t)resp, sizeof(virtio_gpu_resp_display_info_t));

    if (resp->hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO)
    {
        LOG_WARNING("virtio gpu: invalid display info");
        return -RES_EUNKNOWN;
    }

    memcpy(out, resp, sizeof(virtio_gpu_resp_display_info_t));

    pmm_free((uint64_t *)cmd);
    pmm_free((uint64_t *)resp);

    return RES_SUCCESS;
}

static int resource_create_2d(virtqueue_t *vq, virtio_resource_2d_t *resource)
{
    virtio_gpu_resource_create_2d_t *cmd = (virtio_gpu_resource_create_2d_t *)pmm_alloc();
    memset(cmd, 0, sizeof(virtio_gpu_resource_create_2d_t));
    cmd->hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    cmd->format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    cmd->resource_id = resource->id;
    cmd->width = resource->width;
    cmd->height = resource->height;

    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)pmm_alloc();

    virtio_send(virtio_dev, vq, (uintptr_t)cmd, sizeof(virtio_gpu_resource_create_2d_t), (uintptr_t)resp, sizeof(virtio_gpu_ctrl_hdr_t));

    if (resp->type != VIRTIO_GPU_RESP_OK_NODATA)
    {
        LOG_WARNING("virtio gpu: failed to create 2d resource");
        return -RES_EUNKNOWN;
    }

    pmm_free((uint64_t *)cmd);
    pmm_free((uint64_t *)resp);

    return RES_SUCCESS;
}

static int resource_attach_backing(virtqueue_t *vq, virtio_resource_2d_t *resource, uintptr_t addr, uint32_t length)
{
    virtio_gpu_resource_attach_backing_t *cmd = (virtio_gpu_resource_attach_backing_t *)pmm_alloc();
    memset(cmd, 0, sizeof(virtio_gpu_resource_attach_backing_t) + sizeof(virtio_gpu_mem_entry_t));
    cmd->hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    cmd->resource_id = resource->id;
    cmd->nr_entries = 1;

    cmd->entries[0].addr = addr;
    cmd->entries[0].length = length;
    cmd->entries[0].padding = 0;

    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)pmm_alloc();

    virtio_send(virtio_dev, vq, (uintptr_t)cmd, sizeof(virtio_gpu_resource_attach_backing_t) + sizeof(virtio_gpu_mem_entry_t), (uintptr_t)resp, sizeof(virtio_gpu_ctrl_hdr_t));

    if (resp->type != VIRTIO_GPU_RESP_OK_NODATA)
    {
        LOG_WARNING("virtio gpu: failed to attach backing");
        return -RES_EUNKNOWN;
    }

    pmm_free((uint64_t *)cmd);
    pmm_free((uint64_t *)resp);

    return RES_SUCCESS;
}

static int set_scanout(virtqueue_t *vq, virtio_resource_2d_t *resource, virtio_gpu_rect_t *rect)
{
    virtio_gpu_set_scanout_t *cmd = (virtio_gpu_set_scanout_t *)pmm_alloc();
    memset(cmd, 0, sizeof(virtio_gpu_set_scanout_t));
    cmd->hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    cmd->resource_id = resource->id;
    cmd->scanout_id = 0;
    cmd->rect = *rect;

    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)pmm_alloc();

    virtio_send(virtio_dev, vq, (uintptr_t)cmd, sizeof(virtio_gpu_set_scanout_t), (uintptr_t)resp, sizeof(virtio_gpu_ctrl_hdr_t));

    if (resp->type != VIRTIO_GPU_RESP_OK_NODATA)
    {
        LOG_WARNING("virtio gpu: failed to attach backing");
        return -RES_EUNKNOWN;
    }

    pmm_free((uint64_t *)cmd);
    pmm_free((uint64_t *)resp);

    return RES_SUCCESS;
}

static int transfer_to_host(virtqueue_t *vq, virtio_resource_2d_t *resource)
{
    virtio_gpu_transfer_to_host_2d_t *cmd = (virtio_gpu_transfer_to_host_2d_t *)pmm_alloc();
    memset(cmd, 0, sizeof(virtio_gpu_transfer_to_host_2d_t));
    cmd->hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    cmd->resource_id = resource->id;
    cmd->offset = 0;

    cmd->rect.width = resource->width;
    cmd->rect.height = resource->height;
    cmd->rect.x = 0;
    cmd->rect.y = 0;

    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)pmm_alloc();

    virtio_send(virtio_dev, vq, (uintptr_t)cmd, sizeof(virtio_gpu_transfer_to_host_2d_t), (uintptr_t)resp, sizeof(virtio_gpu_ctrl_hdr_t));

    if (resp->type != VIRTIO_GPU_RESP_OK_NODATA)
    {
        LOG_WARNING("virtio gpu: failed to attach backing");
        return -RES_EUNKNOWN;
    }

    pmm_free((uint64_t *)cmd);
    pmm_free((uint64_t *)resp);

    return RES_SUCCESS;
}

static int flush_resource(virtqueue_t *vq, virtio_resource_2d_t *resource)
{
    virtio_gpu_resource_flush_t *cmd = (virtio_gpu_resource_flush_t *)pmm_alloc();
    memset(cmd, 0, sizeof(virtio_gpu_resource_flush_t));
    cmd->hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    cmd->resource_id = resource->id;

    cmd->rect.width = resource->width;
    cmd->rect.height = resource->height;
    cmd->rect.x = 0;
    cmd->rect.y = 0;

    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)pmm_alloc();

    virtio_send(virtio_dev, vq, (uintptr_t)cmd, sizeof(*cmd), (uintptr_t)resp, sizeof(*resp));

    if (resp->type != VIRTIO_GPU_RESP_OK_NODATA)
    {
        LOG_WARNING("virtio gpu: flush failed");
        return -RES_EUNKNOWN;
    }

    pmm_free((uint64_t *)cmd);
    pmm_free((uint64_t *)resp);

    return RES_SUCCESS;
}

static void virtio_video_irq(interrupt_frame_t *frame)
{
    (void)frame;
    uint8_t isr_status = *(volatile uint8_t *)virtio_dev->isr;
}

extern driver_t virtio_video_driver;
device_ops_t virtio_video_ops = {
    .free = &virtio_video_free,
    .get_display_rect = &virtio_video_get_display_rect,
};

device_t *virtio_video_create(size_t index, pci_device_t *pci_dev)
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
    device->driver = &virtio_video_driver;
    device->ops = &virtio_video_ops;
    device->pci_dev = pci_dev;

    pci_enable_device(pci_dev);

    register_interrupt_handler(43, &virtio_video_irq);

    virtio_dev = virtio_init(pci_dev);
    virtqueue_t *command_queue = virtio_setup_queue(virtio_dev, 0);
    virtio_start(virtio_dev);

    if (get_display_info(command_queue, &display_info) < 0)
    {
        return NULL;
    }

    if (!display_info.pmodes[0].enabled)
    {
        LOG_WARNING("virtio gpu: no display connected");
        return NULL;
    }

    virtio_resource_2d_t framebuf_resource = {
        .width = display_info.pmodes[0].rect.width,
        .height = display_info.pmodes[0].rect.height,
        .id = 1,
    };

    if (resource_create_2d(command_queue, &framebuf_resource) < 0)
    {
        return NULL;
    }

    size_t framebuf_size = display_info.pmodes[0].rect.width * display_info.pmodes[0].rect.height * 4;
    uint32_t *framebuf = pmm_alloc_contiguous(framebuf_size / PAGE_SIZE + 1);

    if (resource_attach_backing(command_queue, &framebuf_resource, (uintptr_t)framebuf, framebuf_size) < 0)
    {
        return NULL;
    }

    virtio_gpu_rect_t scanout_rect = {
        .width = framebuf_resource.width,
        .height = framebuf_resource.height,
        .x = 0,
        .y = 0,
    };

    if (set_scanout(command_queue, &framebuf_resource, &scanout_rect) < 0)
    {
        return NULL;
    }

    for (size_t y = 0; y < framebuf_resource.height; ++y)
    {
        for (size_t x = 0; x < framebuf_resource.width; ++x)
        {
            uint8_t r = (uint8_t)((x * 255) / (framebuf_resource.width - 1));
            uint8_t g = (uint8_t)((y * 255) / (framebuf_resource.height - 1));
            uint8_t b = 128;
            uint8_t a = 255;

            framebuf[y * framebuf_resource.width + x] =
                ((uint32_t)a << 24) |
                ((uint32_t)b << 16) |
                ((uint32_t)g << 8) |
                ((uint32_t)r);
        }
    }

    if (transfer_to_host(command_queue, &framebuf_resource) < 0)
    {
        return NULL;
    }

    if (flush_resource(command_queue, &framebuf_resource) < 0)
    {
        return NULL;
    }

    return device;
}
