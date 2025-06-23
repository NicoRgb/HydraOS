#include <kernel/dev/chardev.h>
#include <stddef.h>

chardev_t *chardev_new_ref(chardev_t *cdev)
{
    if (!cdev)
    {
        return NULL;
    }

    cdev->references++;
    return cdev;
}

KRES chardev_free_ref(chardev_t *cdev)
{
    if (!cdev || !cdev->free)
    {
        return -RES_INVARG;
    }

    if (cdev->references <= 1)
    {
        return cdev->free(cdev);
    }
    cdev->references--;

    return 0;
}

KRES chardev_write(char c, chardev_color_t fg, chardev_color_t bg, chardev_t *cdev)
{
    if (!cdev || !cdev->write)
    {
        return -RES_INVARG;
    }

    return cdev->write(c, fg, bg, cdev);
}
