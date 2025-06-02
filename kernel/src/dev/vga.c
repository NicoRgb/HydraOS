#include <kernel/dev/chardev.h>
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

int vga_write(char c, chardev_color_t fg, chardev_color_t bg, chardev_t *cdev)
{
    if (!cdev)
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
            if ((res = vga_write(' ', fg, bg, cdev)) != 0)
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

int vga_free(chardev_t *cdev)
{
    if (!cdev)
    {
        return -RES_INVARG;
    }

    kfree(cdev);

    return 0;
}

chardev_t *vga_create(void)
{
    if (vga_initialized)
    {
        return NULL;
    }

    chardev_t *cdev = kmalloc(sizeof(chardev_t));
    if (!cdev)
    {
        return NULL;
    }
    vga_initialized = true;

    cdev->write = &vga_write;
    cdev->free = &vga_free;
    cdev->references = 1;

    // clearing the screen
    for (size_t row = 0; row < NUM_ROWS; row++)
    {
        for (size_t col = 0; col < NUM_COLS; col++)
        {
            vga_char_mem[col + row * NUM_COLS] = 0x0000;
        }
    }

    return cdev;
}
