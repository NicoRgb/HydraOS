#include <kernel/dev/blockdev.h>
#include <kernel/port.h>
#include <kernel/kmm.h>
#include <kernel/dev/pci.h>
#include <kernel/pit.h>
#include <kernel/string.h>
#include <kernel/isr.h>

/*
TODO:
 - dma support
 - atapi write
*/

#define ATA_SR_BSY 0x80  // Busy
#define ATA_SR_DRDY 0x40 // Drive ready
#define ATA_SR_DF 0x20   // Drive write fault
#define ATA_SR_DSC 0x10  // Drive seek complete
#define ATA_SR_DRQ 0x08  // Data request ready
#define ATA_SR_CORR 0x04 // Corrected data
#define ATA_SR_IDX 0x02  // Index
#define ATA_SR_ERR 0x01  // Error

#define ATA_ER_BBK 0x80   // Bad block
#define ATA_ER_UNC 0x40   // Uncorrectable data
#define ATA_ER_MC 0x20    // Media changed
#define ATA_ER_IDNF 0x10  // ID mark not found
#define ATA_ER_MCR 0x08   // Media change request
#define ATA_ER_ABRT 0x04  // Command aborted
#define ATA_ER_TK0NF 0x02 // Track 0 not found
#define ATA_ER_AMNF 0x01  // No address mark

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_READ_PIO_EXT 0x24
#define ATA_CMD_READ_DMA 0xC8
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_WRITE_PIO_EXT 0x34
#define ATA_CMD_WRITE_DMA 0xCA
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_PACKET 0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY 0xEC

#define ATAPI_CMD_READ 0xA8
#define ATAPI_CMD_EJECT 0x1B

#define ATA_IDENT_DEVICETYPE 0
#define ATA_IDENT_CYLINDERS 2
#define ATA_IDENT_HEADS 6
#define ATA_IDENT_SECTORS 12
#define ATA_IDENT_SERIAL 20
#define ATA_IDENT_MODEL 54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID 106
#define ATA_IDENT_MAX_LBA 120
#define ATA_IDENT_COMMANDSETS 164
#define ATA_IDENT_MAX_LBA_EXT 200

#define IDE_ATA 0x00
#define IDE_ATAPI 0x01

#define ATA_MASTER 0x00
#define ATA_SLAVE 0x01

#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCOUNT0 0x02
#define ATA_REG_LBA0 0x03
#define ATA_REG_LBA1 0x04
#define ATA_REG_LBA2 0x05
#define ATA_REG_HDDEVSEL 0x06
#define ATA_REG_COMMAND 0x07
#define ATA_REG_STATUS 0x07
#define ATA_REG_SECCOUNT1 0x08
#define ATA_REG_LBA3 0x09
#define ATA_REG_LBA4 0x0A
#define ATA_REG_LBA5 0x0B
#define ATA_REG_CONTROL 0x0C
#define ATA_REG_ALTSTATUS 0x0C
#define ATA_REG_DEVADDRESS 0x0D

// Channels:
#define ATA_PRIMARY 0x00
#define ATA_SECONDARY 0x01

// Directions:
#define ATA_READ 0x00
#define ATA_WRITE 0x01

typedef struct
{
    uint16_t base;  // I/O Base
    uint16_t ctrl;  // Control Base
    uint16_t bmide; // Bus Master IDE
    uint8_t nIEN;   // nIEN (No Interrupt);
} IDE_channel_registers_t;

IDE_channel_registers_t channels[2];

typedef struct
{
    uint8_t reserved;      // 0 (Empty) or 1 (This Drive really exists)
    uint8_t channel;       // 0 (Primary Channel) or 1 (Secondary Channel)
    uint8_t drive;         // 0 (Master Drive) or 1 (Slave Drive)
    uint16_t type;         // 0: ATA, 1:ATAPI
    uint16_t signature;    // Drive Signature
    uint16_t capabilities; // Features
    uint32_t command_sets; // Command Sets Supported
    uint32_t size;         // Size in Sectors
    uint8_t model[41];     // Model in string
} ide_device_t;

ide_device_t ide_devices[4];

uint8_t ide_buf[2048] = {0};
static volatile uint8_t ide_irq_invoked = 0;
static uint8_t atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void ide_wait_irq()
{
    while (!ide_irq_invoked);
    ide_irq_invoked = 0;
}

void ide_write(uint8_t channel, uint8_t reg, uint8_t data)
{
    if (reg > 0x07 && reg < 0x0C)
    {
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    }
    if (reg < 0x08)
    {
        port_byte_out(channels[channel].base + reg - 0x00, data);
    }
    else if (reg < 0x0C)
    {
        port_byte_out(channels[channel].base + reg - 0x06, data);
    }
    else if (reg < 0x0E)
    {
        port_byte_out(channels[channel].ctrl + reg - 0x0A, data);
    }
    else if (reg < 0x16)
    {
        port_byte_out(channels[channel].bmide + reg - 0x0E, data);
    }

    if (reg > 0x07 && reg < 0x0C)
    {
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    }
}

uint8_t ide_read(uint8_t channel, uint8_t reg)
{
    uint8_t result;
    if (reg > 0x07 && reg < 0x0C)
    {
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    }
    if (reg < 0x08)
    {
        result = port_byte_in(channels[channel].base + reg - 0x00);
    }
    else if (reg < 0x0C)
    {
        result = port_byte_in(channels[channel].base + reg - 0x06);
    }
    else if (reg < 0x0E)
    {
        result = port_byte_in(channels[channel].ctrl + reg - 0x0A);
    }
    else if (reg < 0x16)
    {
        result = port_byte_in(channels[channel].bmide + reg - 0x0E);
    }

    if (reg > 0x07 && reg < 0x0C)
    {
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    }
    return result;
}

void insl(unsigned reg, uint32_t *buffer, int quads)
{
    int index;
    for (index = 0; index < quads; index++)
    {
        buffer[index] = port_dword_in(reg);
    }
}

void ide_read_buffer(uint8_t channel, uint8_t reg, uint32_t *buffer,
                     uint32_t quads)
{
    if (reg > 0x07 && reg < 0x0C)
    {
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    }
    if (reg < 0x08)
    {
        insl(channels[channel].base + reg - 0x00, buffer, quads);
    }
    else if (reg < 0x0C)
    {
        insl(channels[channel].base + reg - 0x06, buffer, quads);
    }
    else if (reg < 0x0E)
    {
        insl(channels[channel].ctrl + reg - 0x0A, buffer, quads);
    }
    else if (reg < 0x16)
    {
        insl(channels[channel].bmide + reg - 0x0E, buffer, quads);
    }

    if (reg > 0x07 && reg < 0x0C)
    {
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    }
}

uint8_t ide_poll(uint8_t channel, uint32_t advanced_check)
{

    for (int i = 0; i < 4; i++)
    {
        ide_read(channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes 100ns; loop four times
    }

    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
        ; // Wait for BSY to be zero

    if (advanced_check)
    {
        uint8_t state = ide_read(channel, ATA_REG_STATUS);
        if (state & ATA_SR_ERR)
        {
            return 2; // error
        }

        if (state & ATA_SR_DF)
        {
            return 1; // device fault
        }

        if ((state & ATA_SR_DRQ) == 0)
        {
            return 3; // DRQ should be set
        }
    }

    return 0;
}

static uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint64_t lba, uint8_t numsects, const uint8_t *buf_in, uint8_t *buf_out)
{
    uint8_t lba_mode, dma, cmd;
    uint8_t lba_io[6];
    uint32_t channel = ide_devices[drive].channel;
    uint32_t slavebit = ide_devices[drive].drive;
    uint32_t bus = channels[channel].base;
    uint32_t words = 256;
    uint16_t cyl, i;
    uint8_t head, sect, err;

    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = (ide_irq_invoked = 0x0) + 0x02);

    if (lba >= 0x10000000)
    {
        // LBA48
        lba_mode = 2;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = (lba & 0xFF000000) >> 24;
        lba_io[4] = (lba & 0xFF000000) >> 32;
        lba_io[5] = (lba & 0xFF000000) >> 48;
        head = 0;
    }
    else if (ide_devices[drive].capabilities & 0x200)
    {
        // LBA28
        lba_mode = 1;
        lba_io[0] = (lba & 0x00000FF) >> 0;
        lba_io[1] = (lba & 0x000FF00) >> 8;
        lba_io[2] = (lba & 0x0FF0000) >> 16;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba & 0xF000000) >> 24;
    }
    else
    {
        // CHS
        lba_mode = 0;
        sect = (lba % 63) + 1;
        cyl = (lba + 1 - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl >> 0) & 0xFF;
        lba_io[2] = (cyl >> 8) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head = (lba + 1 - sect) % (16 * 63) / (63);
    }

    dma = 0; // not supported

    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY);

    if (lba_mode == 0)
    {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xa0 | (slavebit << 4) | head);
    }
    else
    {
        ide_write(channel, ATA_REG_HDDEVSEL, 0xe0 | (slavebit << 4) | head);
    }

    if (lba_mode == 2)
    {
        ide_write(channel, ATA_REG_SECCOUNT1, 0);
        ide_write(channel, ATA_REG_LBA3, lba_io[3]);
        ide_write(channel, ATA_REG_LBA4, lba_io[4]);
        ide_write(channel, ATA_REG_LBA5, lba_io[5]);
    }
    ide_write(channel, ATA_REG_SECCOUNT0, numsects);
    ide_write(channel, ATA_REG_LBA0, lba_io[0]);
    ide_write(channel, ATA_REG_LBA1, lba_io[1]);
    ide_write(channel, ATA_REG_LBA2, lba_io[2]);

    if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;   
    if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;   
    if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
    ide_write(channel, ATA_REG_COMMAND, cmd);

    if (direction == 0)
    {
        for (i = 0; i < numsects; i++)
        {
            err = ide_poll(channel, 1);
            if (err)
            {
                return err;
            }

            for (uint32_t j = 0; j < words; j++)
            {
                *((uint16_t *)buf_out) = port_word_in(bus);
                buf_out += 2;
            }
        }
    }
    else
    {
        for (i = 0; i < numsects; i++)
        {
            err = ide_poll(channel, 0);
            if (err)
            {
                return err;
            }

            for (uint32_t j = 0; j < words; j++)
            {
                port_word_out(bus, *((uint16_t *)buf_in));
                buf_in += 2;
            }

            ide_write(channel, ATA_REG_COMMAND, (char []) { ATA_CMD_CACHE_FLUSH,
                    ATA_CMD_CACHE_FLUSH,
                    ATA_CMD_CACHE_FLUSH_EXT }[lba_mode]);
            ide_poll(channel, 0);
        }
    }

    return 0;
}

uint8_t ide_atapi_read(uint8_t drive, uint32_t lba, uint8_t numsects, uint8_t *buf)
{
    uint32_t channel = ide_devices[drive].channel;
    uint32_t slavebit = ide_devices[drive].drive;
    uint32_t bus = channels[channel].base;
    uint32_t words = 1024;
    uint8_t err = 0;
    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = ide_irq_invoked = 0x0); // enable irq

    atapi_packet[0] = ATAPI_CMD_READ;
    atapi_packet[1] = 0x0;
    atapi_packet[2] = (lba >> 24) & 0xFF;
    atapi_packet[3] = (lba >> 16) & 0xFF;
    atapi_packet[4] = (lba >> 8) & 0xFF;
    atapi_packet[5] = (lba >> 0) & 0xFF;
    atapi_packet[6] = 0x0;
    atapi_packet[7] = 0x0;
    atapi_packet[8] = 0x0;
    atapi_packet[9] = numsects;
    atapi_packet[10] = 0x0;
    atapi_packet[11] = 0x0;

    ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);
    for (int i = 0; i < 4; i++)
    {
        ide_read(channel, ATA_REG_ALTSTATUS);
    }

    ide_write(channel, ATA_REG_FEATURES, 0);
    ide_write(channel, ATA_REG_LBA1, (words * 2) & 0xFF);
    ide_write(channel, ATA_REG_LBA2, (words * 2) >> 8);

    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

    err = ide_poll(channel, 1);
    if (err)
    {
        return err;
    }

    __asm__("rep outsw" : : "c"(6), "d"(bus), "S"(atapi_packet));

    for (int i = 0; i < numsects; i++)
    {
        ide_wait_irq();

        err = ide_poll(channel, 1);
        if (err)
        {
            return err;
        }

        __asm__ volatile("rep insw"
                        : "+D" (buf), "+c" (words)
                        : "d" (bus)
                        : "memory");
        buf += (words * 2);
    }

    ide_wait_irq();
    while (ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));

    return 0;
}

static void ide_initialize(uint32_t primary_base, uint32_t primary_control_base, uint32_t secondary_base, uint32_t secondary_control_base, uint32_t bus_master_ide)
{
    int j, k, count = 0;

    channels[ATA_PRIMARY].base = (primary_base & 0xFFFFFFFC) + 0x1F0 * (!primary_base);
    channels[ATA_PRIMARY].ctrl = (primary_control_base & 0xFFFFFFFC) + 0x3F6 * (!primary_control_base);
    channels[ATA_SECONDARY].base = (secondary_base & secondary_base) + 0x170 * (!secondary_base);
    channels[ATA_SECONDARY].ctrl = (secondary_control_base & 0xFFFFFFFC) + 0x376 * (!secondary_control_base);
    channels[ATA_PRIMARY].bmide = (bus_master_ide & 0xFFFFFFFC) + 0;
    channels[ATA_SECONDARY].bmide = (bus_master_ide & 0xFFFFFFFC) + 8;

    // disable interrupts
    ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
    ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

    for (int i = 0; i < 2; i++)
    {
        for (j = 0; j < 2; j++)
        {
            uint8_t err = 0, type = IDE_ATA, status;
            ide_devices[count].reserved = 0;

            ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // select drive
            sleep(1);

            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY); // identify command
            sleep(1);

            if (ide_read(i, ATA_REG_STATUS) == 0)
            {
                continue; // device not present
            }

            while (1)
            {
                status = ide_read(i, ATA_REG_STATUS);
                if ((status & ATA_SR_ERR))
                {
                    err = 1; // not an ATA device
                    break;
                }
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
                {
                    break;
                }
            }

            // ATAPI device
            if (err != 0)
            {
                uint8_t cl = ide_read(i, ATA_REG_LBA1);
                uint8_t ch = ide_read(i, ATA_REG_LBA2);

                if (cl == 0x14 && ch == 0xEB)
                {
                    type = IDE_ATAPI;
                }
                else if (cl == 0x69 && ch == 0x96)
                {
                    type = IDE_ATAPI;
                }
                else
                {
                    continue; // unknown type
                }

                ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                sleep(1);
            }

            ide_read_buffer(i, ATA_REG_DATA, (uint32_t *)ide_buf, 128);

            ide_devices[count].reserved = 1;
            ide_devices[count].type = type;
            ide_devices[count].channel = i;
            ide_devices[count].drive = j;
            ide_devices[count].signature = *((uint16_t *)(ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[count].capabilities = *((uint16_t *)(ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[count].command_sets = *((uint32_t *)(ide_buf + ATA_IDENT_COMMANDSETS));

            if (ide_devices[count].command_sets & (1 << 26))
            {
                // 48-bit addressing:
                ide_devices[count].size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
            }
            else
            {
                // CHS or 28-bit addressing:
                ide_devices[count].size = *((uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA));
            }

            for (k = 0; k < 40; k += 2)
            {
                ide_devices[count].model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                ide_devices[count].model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }
            ide_devices[count].model[40] = 0;

            for (int i = 40; i >= 0; i--)
            {
                if (ide_devices[count].model[i] != ' ' && ide_devices[count].model[i] != 0)
                {
                    break;
                }
                ide_devices[count].model[i] = 0;
            }

            count++;
        }
    }
}

void ide_irq(interrupt_frame_t *frame)
{
    (void)frame;
    ide_irq_invoked = 1;
}

int ide_read_block(uint64_t lba, uint8_t *data, blockdev_t *bdev)
{
    if (!bdev || !bdev->available)
    {
        return -RES_INVARG;
    }

    ide_device_t *dev = &ide_devices[bdev->_data];
    if (bdev->_data > 3 || dev->reserved == 0)
    {
        return -RES_INVARG;
    }
    if (((lba + 1) > dev->size) && (dev->type == IDE_ATA))
    {
        return -RES_EUNKNOWN;
    }

    uint8_t err = 0;
    if (dev->type == IDE_ATA)
    {
        err = ide_ata_access(ATA_READ, bdev->_data, lba, 1, NULL, data);
    }
    else if (dev->type == IDE_ATAPI)
    {
        err = ide_atapi_read(bdev->_data, lba, 1, data);
    }

    return -err; // TODO: use own error codes
}

int ide_write_block(uint64_t lba, const uint8_t *data, blockdev_t *bdev)
{
    if (!bdev || !bdev->available)
    {
        return -RES_INVARG;
    }

    ide_device_t *dev = &ide_devices[bdev->_data];

    if (bdev->_data > 3 || dev->reserved == 0)
    {
        return -RES_INVARG;
    }

    if (((lba + 1) > dev->size) && (dev->type == IDE_ATA))
    {
        return -RES_EUNKNOWN;
    }

    uint8_t err = 0;
    if (dev->type == IDE_ATA)
    {
        err = ide_ata_access(ATA_WRITE, bdev->_data, lba, 1, data, NULL);
    }
    else if (dev->type == IDE_ATAPI)
    {
        return -RES_EUNKNOWN;
    }

    return -err; // TODO: use own error codes
}

int ide_eject(blockdev_t *bdev)
{
    if (!bdev)
    {
        return -RES_INVARG;
    }
    if (ide_devices[bdev->_data].type != IDE_ATAPI)
    {
        return -RES_EUNKNOWN;
    }

    // TODO

    return 0;
}

int ide_free(blockdev_t *bdev)
{
    if (!bdev)
    {
        return -RES_INVARG;
    }

    kfree(bdev);

    return 0;
}

#define PROG_IF_PRIMARY_NATIVE_MODE 0b00000001
#define PROG_IF_SECONDARY_NATIVE_MODE 0b00000100
#define PROG_IF_DMA_SUPPORT 0b10000000

static bool ide_initialized = false;

blockdev_t *ide_create(size_t index, pci_device_t *pci_dev)
{
    if (index >= 4)
    {
        return NULL;
    }

    if (!ide_initialized)
    {
        if (register_interrupt_handler(14, &ide_irq) < 0)
        {
            return NULL;
        }

        if (register_interrupt_handler(15, &ide_irq) < 0)
        {
            return NULL;
        }

        uint32_t primary_base = 0x1f0;
        uint32_t primary_control_base = 0x3f6;
        uint32_t secondary_base = 0x170;
        uint32_t secondary_control_base = 0x376;

        if ((pci_dev->prog_if & PROG_IF_PRIMARY_NATIVE_MODE) == PROG_IF_PRIMARY_NATIVE_MODE)
        {
            primary_base = pci_dev->bars[0].address;
            primary_control_base = pci_dev->bars[1].address;
        }
        if ((pci_dev->prog_if & PROG_IF_SECONDARY_NATIVE_MODE) == PROG_IF_SECONDARY_NATIVE_MODE)
        {
            secondary_base = pci_dev->bars[2].address;
            secondary_control_base = pci_dev->bars[3].address;
        }

        ide_initialize(primary_base, primary_control_base, secondary_base, secondary_control_base, 0x00);
        ide_initialized = true;
    }

    ide_device_t *dev = &ide_devices[index];

    if (dev->size == 0)
    {
        return NULL;
    }

    blockdev_t *bdev = kmalloc(sizeof(blockdev_t));
    if (!bdev)
    {
        return NULL;
    }

    bdev->free = &ide_free;
    bdev->read_block = &ide_read_block;
    bdev->write_block = &ide_write_block;
    bdev->eject = &ide_eject;
    bdev->block_size = dev->type == IDE_ATA ? 512 : 2048;
    bdev->num_blocks = dev->size;
    bdev->_data = index;
    strncpy(bdev->model, (char *)dev->model, BLOCKDEV_MODEL_MAX_LEN);
    bdev->type = BLOCKDEV_TYPE_HARD_DRIVE;
    if (dev->type == IDE_ATAPI)
    {
        bdev->type = BLOCKDEV_TYPE_REMOVABLE;
    }
    bdev->available = true;
    bdev->references = 1;

    return bdev;
}
