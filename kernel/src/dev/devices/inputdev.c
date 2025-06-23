#include <kernel/dev/inputdev.h>
#include <stddef.h>

inputdev_t *inputdev_new_ref(inputdev_t *idev)
{
    if (!idev)
    {
        return NULL;
    }

    idev->references++;
    return idev;
}

int inputdev_free_ref(inputdev_t *idev)
{
    if (!idev || !idev->free)
    {
        return -RES_INVARG;
    }

    if (idev->references <= 1)
    {
        return idev->free(idev);
    }
    idev->references--;

    return 0;
}

int inputdev_poll(inputpacket_t *packet, inputdev_t *idev)
{
    if (!idev || !idev->poll)
    {
        return -RES_INVARG;
    }

    return idev->poll(packet, idev);
}

const char qwertz_normal[128] = {
    /* 0x00 */ 0, 0, '1', '2', '3', '4', '5', '6',
    /* 0x08 */ '7', '8', '9', '0', 0, 0, '\b', 0,
    /* 0x10 */ 'q', 'w', 'e', 'r', 't', 'z', 'u', 'i',
    /* 0x18 */ 'o', 'p', 0, '+', '\n', 0, 'a', 's',
    /* 0x20 */ 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0,
    /* 0x28 */ 0, '^', 0, '#', 'y', 'x', 'c', 'v',
    /* 0x30 */ 'b', 'n', 'm', ',', '.', '-', 0, 0, 0, ' ',
};

const char qwertz_shift[128] = {
    /* 0x00 */ 0, 0, '!', '"', 0, '$', '%', '&',
    /* 0x08 */ '/', '(', ')', '=', 0, 0, '\b', 0,
    /* 0x10 */ 'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
    /* 0x18 */ 'O', 'P', 0, '*', '\n', 0, 'A', 'S',
    /* 0x20 */ 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0,
    /* 0x28 */ 0, 0, 0, '\'', 'Y', 'X', 'C', 'V',
    /* 0x30 */ 'B', 'N', 'M', ';', ':', '_', 0, 0, 0, ' ',
};

const char qwertz_caps[128] = {
    /* 0x00 */ 0, 0, '1', '2', '3', '4', '5', '6',
    /* 0x08 */ '7', '8', '9', '0', 0, 0, '\b', 0,
    /* 0x10 */ 'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I',
    /* 0x18 */ 'O', 'P', 0, '+', '\n', 0, 'A', 'S',
    /* 0x20 */ 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0,
    /* 0x28 */ 0, '^', 0, '#', 'Y', 'X', 'C', 'V',
    /* 0x30 */ 'B', 'N', 'M', ',', '.', '-', 0, 0, 0, ' ',
};

char inputdev_packet_to_ascii(inputpacket_t *packet)
{
    if (packet->scancode >= 128)
    {
        return 0; // Invalid scancode
    }

    char ascii_char;

    // Check if Shift or Caps Lock is active
    if ((packet->modifier & MODIFIER_SHIFT) && (packet->modifier & MODIFIER_CAPS_LOCK))
    {
        // Both Shift and Caps Lock active
        ascii_char = qwertz_normal[packet->scancode];
        if (ascii_char >= 'a' && ascii_char <= 'z')
        {
            ascii_char -= 32; // Convert to uppercase
        }
        else if (ascii_char >= 'A' && ascii_char <= 'Z')
        {
            ascii_char += 32; // Convert to lowercase
        }
    }
    else if (packet->modifier & MODIFIER_SHIFT)
    {
        ascii_char = qwertz_shift[packet->scancode];
    }
    else if (packet->modifier & MODIFIER_CAPS_LOCK)
    {
        ascii_char = qwertz_caps[packet->scancode];
    }
    else
    {
        ascii_char = qwertz_normal[packet->scancode];
    }

    // Additional handling for Ctrl, Alt can be added if necessary
    if (packet->modifier & MODIFIER_CTRL)
    {
        // Implement Ctrl-specific behavior if needed
    }
    if (packet->modifier & MODIFIER_ALT)
    {
        // Implement Alt-specific behavior if needed
    }

    return ascii_char;
}
