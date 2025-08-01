#include <kernel/dev/pci.h>
#include <kernel/port.h>
#include <kernel/kmm.h>

#define PCI_DATA_PORT 0xCFC
#define PCI_COMMAND_PORT 0xCF8

static pci_device_t *pci_devices = NULL;
static size_t pci_device_count = 0;
static size_t pci_device_capacity = 0;

uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t id = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    port_dword_out(PCI_COMMAND_PORT, id);
    return port_dword_in(PCI_DATA_PORT);
}

static void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    uint32_t id = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    port_dword_out(PCI_COMMAND_PORT, id);
    port_dword_out(PCI_DATA_PORT, value);
}

uint16_t pci_read_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t val = pci_read(bus, device, function, offset & 0xFC);
    uint8_t shift = (offset & 2) * 8;
    return (val >> shift) & 0xFFFF;
}

void pci_write_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value)
{
    uint32_t old = pci_read(bus, device, function, offset & 0xFC);
    uint8_t shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFF << shift;
    uint32_t new_val = (old & ~mask) | ((uint32_t)value << shift);
    pci_write(bus, device, function, offset & 0xFC, new_val);
}

static bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function)
{
    uint32_t data = pci_read(bus, device, function, 0x00);
    uint16_t vendor_id = data & 0xFFFF;
    return vendor_id != 0xFFFF && vendor_id != 0x0000;
}

static void populate_base_address_register(base_address_register_t *bar, uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_num)
{
    uint32_t bar_value = pci_read(bus, device, function, 0x10 + 4 * bar_num);
    bar->type = (bar_value & 0x01) ? BAR_TYPE_INPUT_OUTPUT : BAR_TYPE_MEMORY_MAPPING;

    if (bar->type == BAR_TYPE_MEMORY_MAPPING)
    {
        bar->prefetchable = (bar_value & 0x08) != 0;
        bar->address = bar_value & ~0x0F;
    }
    else
    {
        bar->address = bar_value & ~0x03;
        bar->prefetchable = false;
    }

    pci_write(bus, device, function, 0x10 + 4 * bar_num, 0xFFFFFFFF);
    uint32_t size = pci_read(bus, device, function, 0x10 + 4 * bar_num);

    if (bar->type == BAR_TYPE_MEMORY_MAPPING)
    {
        bar->size = ~(size & ~0x0F) + 1;
    }
    else
    {
        bar->size = ~(size & ~0x03) + 1;
    }

    pci_write(bus, device, function, 0x10 + 4 * bar_num, bar_value);
}

static int add_pci_device(uint8_t bus, uint8_t device, uint8_t function)
{
    if (pci_device_count >= pci_device_capacity)
    {
        size_t pci_device_capacity_old = pci_device_capacity;
        pci_device_capacity = pci_device_capacity ? pci_device_capacity * 2 : 64;
        pci_devices = krealloc(pci_devices, pci_device_capacity_old, pci_device_capacity * sizeof(pci_device_t));
        if (!pci_devices)
        {
            return -RES_NOMEM;
        }
    }

    pci_device_t *dev = &pci_devices[pci_device_count++];
    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    uint32_t vendor_device = pci_read(bus, device, function, 0x00);
    dev->vendor_id = vendor_device & 0xFFFF;
    dev->device_id = (vendor_device >> 16) & 0xFFFF;
    uint32_t class_info = pci_read(bus, device, function, 0x08);
    dev->class_code = (class_info >> 24) & 0xFF;
    dev->subclass_code = (class_info >> 16) & 0xFF;
    dev->prog_if = (class_info >> 8) & 0xFF;
    dev->header_type = (pci_read(bus, device, function, 0x0C) >> 16) & 0xFF;

    for (uint8_t bar_num = 0; bar_num < MAX_BARS; bar_num++)
    {
        populate_base_address_register(&dev->bars[bar_num], bus, device, function, bar_num);
    }

    LOG_INFO("Discovered PCI Device (vendor: 0x%x, device: 0x%x)", dev->vendor_id, dev->device_id);

    return 0;
}

static int pci_scan_device(uint8_t bus, uint8_t device)
{
    if (!pci_device_exists(bus, device, 0))
    {
        return 0;
    }

    uint8_t header_type = (pci_read(bus, device, 0, 0x0C) >> 16) & 0xFF;
    uint8_t function_count = (header_type & 0x80) ? MAX_FUNCTION : 1;

    for (uint8_t function = 0; function < function_count; function++)
    {
        if (pci_device_exists(bus, device, function))
        {
            int status = add_pci_device(bus, device, function);
            if (status < 0)
            {
                return status;
            }
        }
    }

    return 0;
}

static int pci_scan_bus(uint8_t bus)
{
    for (uint8_t device = 0; device < MAX_DEVICE; device++)
    {
        int status = pci_scan_device(bus, device);
        if (status < 0)
        {
            return status;
        }
    }

    return 0;
}

int pci_init()
{
    pci_device_count = 0;
    pci_device_capacity = 16;
    pci_devices = kmalloc(pci_device_capacity * sizeof(pci_device_t));
    if (!pci_devices)
    {
        return -RES_NOMEM;
    }

    for (uint16_t bus = 0; bus < MAX_BUS; bus++)
    {
        int status = pci_scan_bus(bus);
        if (status < 0)
        {
            return status;
        }
    }

    return 0;
}

void pci_free()
{
    if (pci_devices)
    {
        kfree(pci_devices);
        pci_devices = NULL;
        pci_device_count = 0;
        pci_device_capacity = 0;
    }
}

pci_device_t *pci_get_device(uint64_t id)
{
    if (id >= pci_device_count)
    {
        return NULL;
    }

    return &pci_devices[id];
}

void pci_enable_device(pci_device_t *dev)
{
    uint16_t cmd = pci_read_word(dev->bus, dev->device, dev->function, 0x04);
    cmd |= (1 << 1) | (1 << 2); // Enable Memory Space and Bus Master
    pci_write_word(dev->bus, dev->device, dev->function, 0x04, cmd);
}
