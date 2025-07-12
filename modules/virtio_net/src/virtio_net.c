#include <virtio_net.h>
#include <kernel/dev/virtio.h>
#include <kernel/kmm.h>
#include <kernel/isr.h>
#include <kernel/vec.h>

#define VIRTIO_NET_F_CSUM 0
#define VIRTIO_NET_F_GUEST_CSUM 1
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS 2
#define VIRTIO_NET_F_MTU 3
#define VIRTIO_NET_F_MAC 5
#define VIRTIO_NET_F_GUEST_TSO4 7
#define VIRTIO_NET_F_GUEST_TSO6 8
#define VIRTIO_NET_F_GUEST_ECN 9
#define VIRTIO_NET_F_GUEST_UFO 10
#define VIRTIO_NET_F_HOST_TSO4 11
#define VIRTIO_NET_F_HOST_TSO6 12
#define VIRTIO_NET_F_HOST_ECN 13
#define VIRTIO_NET_F_HOST_UFO 14
#define VIRTIO_NET_F_MRG_RXBUF 15
#define VIRTIO_NET_F_STATUS 16
#define VIRTIO_NET_F_CTRL_VQ 17
#define VIRTIO_NET_F_CTRL_RX 18
#define VIRTIO_NET_F_CTRL_VLAN 19
#define VIRTIO_NET_F_CTRL_RX_EXTRA 20
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21
#define VIRTIO_NET_F_MQ 22
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23
#define VIRTIO_NET_F_HASH_TUNNEL 51
#define VIRTIO_NET_F_VQ_NOTF_COAL 52
#define VIRTIO_NET_F_NOTF_COAL 53
#define VIRTIO_NET_F_GUEST_USO4 54
#define VIRTIO_NET_F_GUEST_USO6 55
#define VIRTIO_NET_F_HOST_USO 56
#define VIRTIO_NET_F_HASH_REPORT 57
#define VIRTIO_NET_F_GUEST_HDRLEN 59
#define VIRTIO_NET_F_RSS 60
#define VIRTIO_NET_F_RSC_EXT 61
#define VIRTIO_NET_F_STANDBY 62
#define VIRTIO_NET_F_SPEED_DUPLEX 63

typedef struct
{
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
    uint32_t speed;
    uint8_t duplex;
    uint8_t rss_max_key_size;
    uint16_t rss_max_indirection_table_length;
    uint32_t supported_hash_types;
    uint32_t supported_tunnel_types;
} virtio_net_config_t;

typedef struct
{
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} virtio_net_hdr_t;

static virtio_device_t *virtio_dev = NULL;
static virtio_net_config_t *device_cfg = NULL;
static virtqueue_t *receive_queue = NULL;
static virtqueue_t *transmit_queue = NULL;

int virtio_net_free(device_t *dev)
{
    (void)dev;
    return 0;
}

static uint32_t virtio_video_feature_negotiate(uint32_t features, virtio_device_t *device, bool *abort)
{
    uint32_t res = features;

    if ((features & VIRTIO_NET_F_MQ) || (features & VIRTIO_NET_F_RSS))
    {
        virtio_net_config_t *device_cfg = (virtio_net_config_t *)device->device_cfg;
        device_cfg->max_virtqueue_pairs = 1;
    }

    if (!(features & VIRTIO_NET_F_MAC) || !(features & VIRTIO_NET_F_MRG_RXBUF)) // required
    {
        *abort = true;
        return 0;
    }

    if (features & VIRTIO_NET_F_STATUS)
    {
        res = res & ~VIRTIO_NET_F_STATUS; // TODO: support this flag
    }

    return res;
}

extern driver_t virtio_net_driver;
device_ops_t virtio_net_ops = {
    .free = &virtio_net_free,
};

device_t *virtio_net_create(size_t index, pci_device_t *pci_dev)
{
    (void)index;

    LOG_WARNING("Net Create");

    if (virtio_dev != NULL)
    {
        PANIC("virtio net initialized twice");
    }

    device_t *device = kmalloc(sizeof(device_t));
    if (!device)
    {
        return NULL;
    }

    device->type = DEVICE_NET;
    device->driver = &virtio_net_driver;
    device->ops = &virtio_net_ops;
    device->pci_dev = pci_dev;

    pci_enable_device(pci_dev);

    virtio_dev = virtio_init(pci_dev, virtio_video_feature_negotiate);
    if (!virtio_dev)
    {
        LOG_WARNING("Device feature negotiation failed");
        kfree(device);
        return NULL;
    }

    receive_queue = virtio_setup_queue(virtio_dev, 0);
    if (!receive_queue)
    {
        return NULL;
    }

    transmit_queue = virtio_setup_queue(virtio_dev, 1);
    if (!transmit_queue)
    {
        return NULL;
    }

    device_cfg = (virtio_net_config_t *)virtio_dev->device_cfg;

    virtio_start(virtio_dev);

    LOG_INFO("Device MAC Address %d%d%d%d%d%d", device_cfg->mac[0], device_cfg->mac[1], device_cfg->mac[2], device_cfg->mac[3], device_cfg->mac[4], device_cfg->mac[5]);

    return device;
}
