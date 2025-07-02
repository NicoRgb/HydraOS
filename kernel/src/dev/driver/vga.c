#include <kernel/dev/devm.h>
#include <kernel/kmm.h>
#include <stddef.h>
#include <stdbool.h>

// there can only be one vga_device
static bool vga_initialized = false;
static size_t col = 0;
static size_t row = 0;

static const size_t NUM_COLS = 80;
static const size_t NUM_ROWS = 25;
static uint16_t *vga_char_mem = (uint16_t *)0xB8000;

static void vga_newline(uint8_t color)
{
    col = 0;
    if (row < NUM_ROWS - 1)
    {
        row++;
        return;
    }

    for (size_t r = 1; r < NUM_ROWS; r++)
    {
        for (size_t c = 0; c < NUM_COLS; c++)
        {
            vga_char_mem[c + (r - 1) * NUM_COLS] = vga_char_mem[c + r * NUM_COLS];
        }
    }

    for (size_t c = 0; c < NUM_COLS; c++)
    {
        vga_char_mem[c + (NUM_ROWS - 1) * NUM_COLS] = (uint16_t)(color << 8);
    }
}

static void vga_backspace(void)
{
    if (col <= 0)
    {
        // TODO: shift upwards
        row--;
        col = NUM_COLS-1;
        return;
    }

    col--;
}

int vga_write(char c, chardev_color_t fg, chardev_color_t bg, device_t *dev)
{
    if (!dev)
    {
        return -RES_INVARG;
    }

    uint8_t color = (fg & 0x0F) | (bg << 4);

    if (c == '\n')
    {
        vga_newline(color);
        return 0;
    }
    if (c == '\b')
    {
        vga_backspace();
        return 0;
    }
    else if (c == '\t')
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            int res = 0;
            if ((res = vga_write(' ', fg, bg, dev)) != 0)
            {
                return res;
            }
        }
        return 0;
    }

    if (col > NUM_COLS)
    {
        vga_newline(color);
    }

    vga_char_mem[col + row * NUM_COLS] = ((uint16_t)(color << 8)) | c;
    col++;

    return 0;
}

int vga_free(device_t *dev)
{
    if (!dev)
    {
        return -RES_INVARG;
    }

    kfree(dev);

    return 0;
}

device_t *vga_create(size_t index, struct _pci_device *pci_dev);

#define PCI_CLASS_DISPLAY_CONTROLLER 0x03
#define PCI_SUBCLASS_VGA_COMP_CONTROLLER 0x00

driver_t vga_driver = {
    .supported_type = DEVICE_CHAR,
    .num_devices = 1,
    .init_device = &vga_create,

    .class_code = PCI_CLASS_DISPLAY_CONTROLLER,
    .subclass_code = PCI_SUBCLASS_VGA_COMP_CONTROLLER,
    .prog_if = 0xFF,
    .vendor_id = 0xFFFF,
    .device_id= 0xFFFF,

    .driver_name = "VGA Compatability Mode Output",
    .device_name = "Virtual Terminal",
    .module = "none",
    .author = "Nico Grundei"
};

device_ops_t vga_ops = {
    .free = &vga_free,
    .write = &vga_write,
};

device_t *vga_create(size_t index, struct _pci_device *pci_dev)
{
    (void)index;
    (void)pci_dev;

    if (vga_initialized)
    {
        return NULL;
    }

    device_t *dev = kmalloc(sizeof(device_t));
    if (!dev)
    {
        return NULL;
    }
    vga_initialized = true;

    dev->type = DEVICE_CHAR;
    dev->driver = &vga_driver;
    dev->ops = &vga_ops;
    dev->pci_dev = pci_dev;

    // clearing the screen
    for (size_t row = 0; row < NUM_ROWS; row++)
    {
        for (size_t col = 0; col < NUM_COLS; col++)
        {
            vga_char_mem[col + row * NUM_COLS] = 0x0000;
        }
    }

    return dev;
}
