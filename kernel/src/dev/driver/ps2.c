#include <kernel/dev/devm.h>
#include <kernel/port.h>
#include <kernel/kmm.h>
#include <kernel/isr.h>
#include <stdbool.h>

static bool ps2_initialized = false;

#define KEY_BUFFER_SIZE 50

typedef struct
{
    uint8_t type;
    uint8_t modifier;
    uint8_t scancode;
} key_stroke_t;

static key_stroke_t key_buffer[KEY_BUFFER_SIZE];
static uint8_t key_buffer_size = 0;

#define NUM_KEYS 256
static bool key_states[NUM_KEYS];

static bool shift_down = false;
static bool ctrl_down = false;
static bool alt_down = false;
static bool caps_lock_down = false;

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_COMMAND_PORT 0x64

void set_keyboard_leds(bool scroll_lock, bool num_lock, bool caps_lock)
{
    uint8_t led_status = 0;

    if (scroll_lock)
    {
        led_status |= 0x01;
    }
    if (num_lock)
    {
        led_status |= 0x02;
    }
    if (caps_lock)
    {
        led_status |= 0x04;
    }

    port_byte_out(KEYBOARD_DATA_PORT, 0xED);
    while (port_byte_in(KEYBOARD_DATA_PORT) != 0xFA);

    port_byte_out(KEYBOARD_DATA_PORT, led_status);
    while (port_byte_in(KEYBOARD_DATA_PORT) != 0xFA);
}

static void keyboard_irq(interrupt_frame_t *frame)
{
    (void)frame;
    key_buffer_size++;

    uint8_t scancode = port_byte_in(KEYBOARD_DATA_PORT);
    bool key_released = scancode & 0x80;
    uint8_t key_code = scancode & 0x7F;

    if (key_released)
    {
        key_buffer[key_buffer_size].type = IPACKET_KEYUP;
        key_states[key_code] = false;

        switch (key_code)
        {
        case 0x2A:
        case 0x36:
            shift_down = false;
            key_buffer_size--;
            return;
        case 0x1D:
            ctrl_down = false;
            key_buffer_size--;
            return;
        case 0x38:
            alt_down = false;
            key_buffer_size--;
            return;
        default:
            break;
        }
    }
    else
    {
        key_buffer[key_buffer_size].type = IPACKET_KEYDOWN;
        if (key_states[key_code])
        {
            key_buffer[key_buffer_size].type = IPACKET_KEYREPEAT;
        }

        key_states[key_code] = true;

        switch (key_code)
        {
        case 0x2A:
        case 0x36:
            shift_down = true;
            key_buffer_size--;
            return;
        case 0x1D:
            ctrl_down = true;
            key_buffer_size--;
            return;
        case 0x38:
            alt_down = true;
            key_buffer_size--;
            return;
        case 0x3A:
            caps_lock_down = !caps_lock_down;
            set_keyboard_leds(false, false, caps_lock_down);
            key_buffer_size--;
            return;
        default:
            break;
        }
    }

    key_buffer[key_buffer_size].scancode = key_code;
    key_buffer[key_buffer_size].modifier = 0;

    if (shift_down)
    {
        key_buffer[key_buffer_size].modifier |= MODIFIER_SHIFT;
    }
    if (ctrl_down)
    {
        key_buffer[key_buffer_size].modifier |= MODIFIER_CTRL;
    }
    if (alt_down)
    {
        key_buffer[key_buffer_size].modifier |= MODIFIER_ALT;
    }
    if (caps_lock_down)
    {
        key_buffer[key_buffer_size].modifier |= MODIFIER_CAPS_LOCK;
    }

    // TODO: handle overflow
}

int ps2_poll(inputpacket_t *packet, device_t *dev)
{
    if (!dev)
    {
        return -RES_INVARG;
    }

    if (key_buffer_size <= 0)
    {
        packet->type = IPACKET_NULL;
        return 0;
    }

    packet->type = key_buffer[key_buffer_size].type;
    packet->modifier = key_buffer[key_buffer_size].modifier;
    packet->scancode = key_buffer[key_buffer_size].scancode;

    key_buffer_size--;
    return 0;
}

int ps2_free(device_t *dev)
{
    if (!dev)
    {
        return -RES_INVARG;
    }

    kfree(dev);

    return 0;
}

device_t *ps2_create(size_t index, struct _pci_device *pci_dev);

driver_t ps2_driver = {
    .supported_type = DEVICE_INPUT,
    .num_devices = 1,
    .init_device = &ps2_create,

    .class_code = 0xFF,
    .subclass_code = 0xFF,
    .prog_if = 0xFF,
    .vendor_id = 0xFFFF,
    .device_id= 0xFFFF,

    .driver_name = "PS2 Controller",
    .device_name = "Keyboard",
    .module = "none",
    .author = "Nico Grundei"
};

device_ops_t ps2_ops = {
    .free = &ps2_free,
    .poll = &ps2_poll,
};

device_t *ps2_create(size_t index, struct _pci_device *pci_dev)
{
    (void)index;
    (void)pci_dev;

    if (ps2_initialized)
    {
        return NULL;
    }

    device_t *dev = kmalloc(sizeof(device_t));
    if (!dev)
    {
        return NULL;
    }
    ps2_initialized = true;

    dev->type = DEVICE_INPUT;
    dev->driver = &ps2_driver;
    dev->ops = &ps2_ops;
    dev->pci_dev = pci_dev;

    register_interrupt_handler(33, &keyboard_irq);

    return dev;
}
