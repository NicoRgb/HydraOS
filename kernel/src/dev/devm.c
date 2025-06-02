#include <kernel/dev/devm.h>
#include <kernel/dev/pci.h>
#include <kernel/kmm.h>

chardev_t *e9_create(void);
chardev_t *vga_create(void);

blockdev_t *ide_create(size_t index, pci_device_t *pci_device);

inputdev_t *ps2_create(void);

#define CHARDEVS_CAPACITY_INCREASE 3

chardev_t **chardevs = NULL;
size_t chardevs_capacity = 0;
size_t chardevs_size = 0;

#define BLOCKDEVS_CAPACITY_INCREASE 3

blockdev_t **blockdevs = NULL;
size_t blockdevs_capacity = 0;
size_t blockdevs_size = 0;

#define INPUTDEVS_CAPACITY_INCREASE 3

inputdev_t **inputdevs = NULL;
size_t inputdevs_capacity = 0;
size_t inputdevs_size = 0;

static KRES init_char_devices(void)
{
    chardevs = kmalloc(sizeof(chardev_t *) * CHARDEVS_CAPACITY_INCREASE);
    if (!chardevs)
    {
        return -RES_NOMEM;
    }

    chardevs_capacity = CHARDEVS_CAPACITY_INCREASE;
    chardevs_size = 0;

    chardevs[chardevs_size] = e9_create();
    if (!chardevs[chardevs_size])
    {
        return -RES_EUNKNOWN;
    }
    chardevs_size++;

    return 0;
}

static KRES init_block_devices(void)
{
    blockdevs = kmalloc(sizeof(blockdev_t *) * BLOCKDEVS_CAPACITY_INCREASE);
    if (!blockdevs)
    {
        return -RES_NOMEM;
    }

    blockdevs_capacity = BLOCKDEVS_CAPACITY_INCREASE;
    blockdevs_size = 0;

    return 0;
}

static KRES init_input_devices(void)
{
    inputdevs = kmalloc(sizeof(inputdev_t *) * INPUTDEVS_CAPACITY_INCREASE);
    if (!inputdevs)
    {
        return -RES_NOMEM;
    }

    inputdevs_capacity = INPUTDEVS_CAPACITY_INCREASE;
    inputdevs_size = 0;

    inputdevs[inputdevs_size] = ps2_create();
    if (!inputdevs[inputdevs_size])
    {
        return -RES_NOMEM;
    }
    inputdevs_size++;

    return 0;
}

static KRES try_init_char_device(pci_device_t *pci_dev)
{
    if (pci_dev->class_code != PCI_CLASS_DISPLAY_CONTROLLER)
    {
        return -RES_INVARG;
    }

    if (pci_dev->subclass_code == PCI_SUBCLASS_VGA_COMP_CONTROLLER)
    {
        if (chardevs_size >= chardevs_capacity)
        {
            chardevs = krealloc(chardevs, chardevs_capacity, chardevs_capacity + sizeof(chardev_t *) * CHARDEVS_CAPACITY_INCREASE);
            chardevs_capacity += CHARDEVS_CAPACITY_INCREASE;
        }

        chardevs[chardevs_size] = vga_create();
        if (!chardevs[chardevs_size])
        {
            return -RES_EUNKNOWN;
        }
        chardevs_size++;
        return 0;
    }

    return 0;
}

static KRES try_init_block_device(pci_device_t *pci_dev)
{
    if (pci_dev->class_code != PCI_CLASS_MASS_STORAGE_CONTROLLER)
    {
        return -RES_INVARG;
    }

    if (pci_dev->subclass_code == PCI_SUBCLASS_IDE_CONTROLLER)
    {
        if (blockdevs_size >= blockdevs_capacity)
        {
            blockdevs = krealloc(blockdevs, blockdevs_capacity, blockdevs_capacity + sizeof(blockdev_t *) * BLOCKDEVS_CAPACITY_INCREASE);
            blockdevs_capacity += BLOCKDEVS_CAPACITY_INCREASE;
        }

        for (size_t i = 0; i < 4; i++)
        {
            blockdevs[blockdevs_size] = ide_create(i, pci_dev);
            if (!blockdevs[blockdevs_size])
            {
                continue;
            }
            blockdevs_size++;
        }
    }

    return 0;
}

KRES init_devices(void)
{
    KRES status = init_char_devices();
    if (status < 0)
    {
        return status;
    }

    status = init_block_devices();
    if (status < 0)
    {
        return status;
    }

    status = init_input_devices();
    if (status < 0)
    {
        return status;
    }

    for (size_t i = 0; i < MAX_PCI_DEVICES; i++)
    {
        pci_device_t *pci_dev = pci_get_device(i);
        if (!pci_dev)
        {
            break;
        }

        if (pci_dev->class_code == PCI_CLASS_DISPLAY_CONTROLLER)
        {
            status = try_init_char_device(pci_dev);
            if (status < 0)
            {
                return status;
            }
        }
        else if (pci_dev->class_code == PCI_CLASS_MASS_STORAGE_CONTROLLER)
        {
            status = try_init_block_device(pci_dev);
            if (status < 0)
            {
                return status;
            }
        }
    }

    return 0;
}

void free_devices(void)
{
    for (size_t i = 0; i < chardevs_size; i++)
    {
        chardev_free_ref(chardevs[i]);
    }

    kfree(chardevs);

    for (size_t i = 0; i < blockdevs_size; i++)
    {
        blockdev_free_ref(blockdevs[i]);
    }

    kfree(blockdevs);
}

chardev_t *get_chardev(size_t index)
{
    if (index >= chardevs_size)
    {
        return NULL;
    }

    return chardevs[index];
}

blockdev_t *get_blockdev(size_t index)
{
    if (index >= blockdevs_size)
    {
        return NULL;
    }

    return blockdevs[index];
}

inputdev_t *get_inputdev(size_t index)
{
    if (index >= inputdevs_size)
    {
        return NULL;
    }

    return inputdevs[index];
}
