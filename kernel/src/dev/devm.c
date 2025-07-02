#include <kernel/dev/devm.h>
#include <kernel/dev/pci.h>
#include <kernel/kmm.h>
#include <kernel/vec.h>

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

cvector(device_t *) devices = CVECTOR;
cvector(driver_t *) driver = CVECTOR;

KRES register_driver(driver_t *d)
{
    if (!d)
    {
        return -RES_INVARG;
    }

    LOG_INFO("Registered Driver '%s' (module %s) by %s", d->driver_name, d->module, d->author);

    cvector_push(driver, d);
    return 0;
}

bool driver_matches(const driver_t *d, const pci_device_t *dev)
{
    if (d->class_code != 0xFF && d->class_code != dev->class_code)
        return false;
    if (d->subclass_code != 0xFF && d->subclass_code != dev->subclass_code)
        return false;
    if (d->prog_if != 0xFF && d->prog_if != dev->prog_if)
        return false;

    if (d->vendor_id != 0xFFFF && d->vendor_id != dev->vendor_id)
        return false;
    if (d->device_id != 0xFFFF && d->device_id != dev->device_id)
        return false;

    return true;
}

KRES driver_instatiate(const driver_t *d, pci_device_t *pci_dev)
{
    for (int k = 0; k < d->num_devices; k++)
    {
        device_t *dev = d->init_device(k, pci_dev);
        if (dev)
        {
            cvector_push(devices, dev);
            LOG_INFO("Initialized Device '%s' from '%s' (module %s) by %s (id 0x%x:0x%x)",
                     d->device_name,
                     d->driver_name,
                     d->module,
                     d->author,
                     dev->pci_dev->vendor_id,
                     dev->pci_dev->device_id);
        }
    }

    return RES_SUCCESS;
}

KRES init_devices(void)
{
    for (size_t j = 0; j < cvector_size(driver); j++)
    {
        if (driver[j]->class_code != 0xFF || driver[j]->vendor_id != 0xFFFF) // uses pci
        {
            continue;
        }

        driver_instatiate(driver[j], NULL);
    }

    for (size_t i = 0; i < MAX_PCI_DEVICES; i++)
    {
        pci_device_t *pci_dev = pci_get_device(i);
        if (!pci_dev)
        {
            break;
        }

        for (size_t j = 0; j < cvector_size(driver); j++)
        {
            if (driver[j]->class_code == 0xFF && driver[j]->vendor_id == 0xFFFF)
            {
                continue;
            }

            if (driver_matches(driver[j], pci_dev))
            {
                driver_instatiate(driver[j], pci_dev);
            }
        }
    }

    return 0;
}

device_t *get_device(uint16_t vendor, uint16_t device)
{
    for (size_t i = 0; i < cvector_size(devices); i++)
    {
        if (devices[i]->pci_dev->vendor_id == vendor && devices[i]->pci_dev->device_id == device)
        {
            return devices[i];
        }
    }

    return NULL;
}

device_t *get_device_by_index(size_t index)
{
    if (index >= cvector_size(devices))
    {
        return NULL;
    }

    return devices[index];
}

device_t *get_inputdev(void)
{
    for (size_t i = 0; i < cvector_size(devices); i++)
    {
        if (devices[i]->type == DEVICE_INPUT)
        {
            return devices[i];
        }
    }

    return NULL;
}

int device_free(device_t *dev)
{
    return dev->ops->free(dev);
}

int device_poll(inputpacket_t *packet, device_t *dev)
{
    return dev->ops->poll(packet, dev);
}

int device_write(char c, chardev_color_t fg, chardev_color_t bg, device_t *dev)
{
    return dev->ops->write(c, fg, bg, dev);
}

int device_read_block(uint64_t lba, uint8_t *data, device_t *dev)
{
    return dev->ops->read_block(lba, data, dev);
}

int device_write_block(uint64_t lba, const uint8_t *data, device_t *dev)
{
    return dev->ops->write_block(lba, data, dev);
}

int device_eject(device_t *dev)
{
    return dev->ops->eject(dev);
}
