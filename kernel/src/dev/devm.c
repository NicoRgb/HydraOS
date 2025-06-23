#include <kernel/dev/devm.h>
#include <kernel/dev/pci.h>
#include <kernel/kmm.h>
#include <kernel/vec.h>

cvector(chardev_t *) chardevs = CVECTOR;
cvector(blockdev_t *) blockdevs = CVECTOR;
cvector(inputdev_t *) inputdevs = CVECTOR;

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
    if (d->class_code != dev->class_code)
    {
        return false;
    }
    if (d->subclass_code != 0xFF && d->subclass_code != dev->subclass_code)
    {
        return false;
    }
    if (d->prog_if != 0xFF && d->prog_if != dev->prog_if)
    {
        return false;
    }

    return true;
}

void driver_instatiate(const driver_t *d)
{
    for (int k = 0; k < d->num_devices; k++)
    {
        switch (d->device_type)
        {
        case DEVICE_TYPE_CHARDEV:
        {
            chardev_t *cdev = d->create_cdev(k, NULL);
            if (cdev)
            {
                LOG_INFO("Initialized Character Device '%s' from '%s' (module %s) by %s",
                         d->device_name,
                         d->driver_name,
                         d->module,
                         d->author);
                cvector_push(chardevs, cdev);
            }
            break;
        }
        case DEVICE_TYPE_BLOCKDEV:
        {
            blockdev_t *bdev = d->create_bdev(k, NULL);
            if (bdev)
            {
                LOG_INFO("Initialized Block Device '%s' from '%s' (module %s) by %s",
                         d->device_name,
                         d->driver_name,
                         d->module,
                         d->author);
                cvector_push(blockdevs, bdev);
            }
            break;
        }
        case DEVICE_TYPE_INPUTDEV:
        {
            inputdev_t *idev = d->create_idev(k, NULL);
            if (idev)
            {
                LOG_INFO("Initialized Input Device '%s' from '%s' (module %s) by %s",
                         d->device_name,
                         d->driver_name,
                         d->module,
                         d->author);
                cvector_push(inputdevs, idev);
            }
            break;
        }
        }
    }
}

KRES init_devices(void)
{
    for (size_t j = 0; j < cvector_size(driver); j++)
    {
        if (driver[j]->class_code != 0xFF) // uses pci
        {
            continue;
        }

        driver_instatiate(driver[j]);
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
            if (driver[j]->class_code == 0xFF)
            {
                continue;
            }

            if (driver_matches(driver[j], pci_dev))
            {
                driver_instatiate(driver[j]);
            }
        }
    }

    return 0;
}

chardev_t *get_chardev(size_t index)
{
    if (index >= cvector_size(chardevs))
    {
        return NULL;
    }

    return chardevs[index];
}

blockdev_t *get_blockdev(size_t index)
{
    if (index >= cvector_size(blockdevs))
    {
        return NULL;
    }

    return blockdevs[index];
}

inputdev_t *get_inputdev(size_t index)
{
    if (index >= cvector_size(inputdevs))
    {
        return NULL;
    }

    return inputdevs[index];
}
