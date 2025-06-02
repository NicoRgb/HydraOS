#ifndef _KERNEL_CHARDEV_H
#define _KERNEL_CHARDEV_H

#include <stdint.h>
#include <kernel/status.h>

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

typedef struct _chardev
{
    int (*write)(char, chardev_color_t, chardev_color_t, struct _chardev *);
    int (*free)(struct _chardev *);
    uint32_t references;
} chardev_t;

chardev_t *chardev_new_ref(chardev_t *cdev);
KRES chardev_free_ref(chardev_t *cdev);

KRES chardev_write(char c, chardev_color_t fg, chardev_color_t bg, chardev_t *cdev);

#endif