#ifndef _KERNEL_DEVM_H
#define _KERNEL_DEVM_H

#include <kernel/status.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define IPACKET_NULL 0
#define IPACKET_KEYDOWN 1
#define IPACKET_KEYREPEAT 2
#define IPACKET_KEYUP 3

#define MODIFIER_SHIFT 1<<0
#define MODIFIER_CTRL 1<<1
#define MODIFIER_ALT 1<<2
#define MODIFIER_CAPS_LOCK 1<<3

typedef enum
{
    CHARDEV_COLOR_BLACK = 0,
    CHARDEV_COLOR_BLUE = 1,
    CHARDEV_COLOR_GREEN = 2,
    CHARDEV_COLOR_CYAN = 3,
    CHARDEV_COLOR_RED = 4,
    CHARDEV_COLOR_MAGENTA = 5,
    CHARDEV_COLOR_BROWN = 6,
    CHARDEV_COLOR_LIGHT_GRAY = 7,
    CHARDEV_COLOR_DARK_GRAY = 8,
    CHARDEV_COLOR_LIGHT_BLUE = 9,
    CHARDEV_COLOR_LIGHT_GREEN = 10,
    CHARDEV_COLOR_LIGHT_CYAN = 11,
    CHARDEV_COLOR_LIGHT_RED = 12,
    CHARDEV_COLOR_PINK = 13,
    CHARDEV_COLOR_YELLOW = 14,
    CHARDEV_COLOR_WHITE = 15,
} chardev_color_t;

typedef struct
{
    uint8_t type;
    uint8_t modifier;
    uint8_t scancode;
    int deltaX;
    int deltaY;
} inputpacket_t;

typedef struct
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) video_rect_t;

char inputdev_packet_to_ascii(inputpacket_t *packet);
size_t get_framebuffer_size(video_rect_t *rect);

struct _pci_device;

typedef enum
{
    DEVICE_BLOCK = 0,
    DEVICE_CHAR = 1,
    DEVICE_INPUT = 2,
    DEVICE_VIDEO = 3,
    DEVICE_RNG = 4,
    DEVICE_NET = 5,
} device_type_t;

#define BLOCKDEV_MODEL_MAX_LEN 41
#define BLOCKDEV_TYPE_HARD_DRIVE 0
#define BLOCKDEV_TYPE_REMOVABLE 1

typedef struct
{
    device_type_t type;
    void *driver_data;
    struct driver *driver;
    struct device_ops *ops;
    struct _pci_device *pci_dev;

    // block device
    uint32_t block_size;
    size_t num_blocks;
    uint8_t blockdev_type;
    char model[BLOCKDEV_MODEL_MAX_LEN];
    bool available;
} device_t;

typedef struct device_ops
{
    int (*free)(device_t *dev);

    // inputdev
    int (*poll)(inputpacket_t *packet, device_t *dev);

    // chardev
    int (*write)(char c, chardev_color_t fg, chardev_color_t bg, device_t *dev);

    // blockdev
    int (*read_block)(uint64_t lba, uint8_t *data, device_t *dev);
    int (*write_block)(uint64_t lba, const uint8_t *data, device_t *dev);
    int (*eject)(device_t *dev);

    // rng
    int (*randomize_buffer)(uint8_t *data, size_t size, device_t *dev);

    // video
    int (*get_display_rect)(video_rect_t *rect, int display, device_t *dev);
    uint32_t *(*create_framebuffer)(video_rect_t *rect, int display, device_t *dev);
    int (*update_display)(video_rect_t *rect, void *framebuffer, device_t *dev);

    // net
    int (*send)(size_t size, const uint8_t *data, device_t *dev);
    int (*recv)(size_t size, uint8_t *data, device_t *dev);
} device_ops_t;

typedef struct driver
{
    // identification
    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t prog_if;

    uint16_t vendor_id;
    uint16_t device_id;

    // logging/debugging
    const char *driver_name;
    const char *device_name;
    const char *module;
    const char *author;

    uint8_t num_devices;
    device_type_t supported_type;

    device_t * (*init_device)(size_t index, struct _pci_device *pci_dev);
} driver_t;

KRES register_driver(driver_t *driver);

KRES init_devices(void);

device_t *get_device(uint16_t vendor, uint16_t device);
device_t *get_device_by_index(size_t index);

device_t *get_device_by_type(device_type_t type, size_t index);

// device function wrapper

int device_free(device_t *dev);
int device_poll(inputpacket_t *packet, device_t *dev);
int device_write(char c, chardev_color_t fg, chardev_color_t bg, device_t *dev);
int device_read_block(uint64_t lba, uint8_t *data, device_t *dev);
int device_write_block(uint64_t lba, const uint8_t *data, device_t *dev);
int device_eject(device_t *dev);
int device_randomize_buffer(uint8_t *data, size_t size, device_t *dev);
int device_get_display_rect(video_rect_t *rect, int display, device_t *dev);
uint32_t *device_create_framebuffer(video_rect_t *rect, int display, device_t *dev);
int device_update_display(video_rect_t *rect, void *framebuffer, device_t *dev);

#endif