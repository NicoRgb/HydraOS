#include <kernel/log.h>
#include <kernel/port.h>
#include <kernel/kprintf.h>
#include <kernel/dbg.h>
#include <kernel/port.h>

#include <stddef.h>
#include <stdarg.h>

#define KLOG_MAX_LENGTH 512

static KRES (*log_write_func)(char c, log_level_t level);

static KRES klog_write_string(const char *str, log_level_t level)
{
    KRES res = RES_SUCCESS;

    for (uint64_t i = 0; str[i] != 0; i++)
    {
        CHECK(log_write_func(str[i], level));
    }

exit:
    return res;
}

KRES klog_write_e9_and_vga(char c, log_level_t level)
{
    KRES res = RES_SUCCESS;

    CHECK(klog_write_e9(c, level));
    CHECK(klog_write_vga(c, level));

exit:
    return res;
}

KRES klog_write_e9(char c, log_level_t level)
{
    (void)level;
    port_byte_out(0xe9, c);
    return RES_SUCCESS;
}

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

KRES klog_write_vga(char c, log_level_t level)
{
    KRES res = RES_SUCCESS;
    uint8_t color = 0x00;

    switch (level)
    {
        case LOG_LEVEL_DEBUG:   color = 0x07; break;
        case LOG_LEVEL_INFO:    color = 0x0A; break;
        case LOG_LEVEL_WARNING: color = 0x0E; break;
        case LOG_LEVEL_ERROR:   color = 0x0C; break;
        default:                res = -RES_INVARG; goto exit;
    }

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
            CHECK(klog_write_vga(' ', level));
        }
        return 0;
    }

    if (col > NUM_COLS)
    {
        vga_newline(color);
    }

    vga_char_mem[col + row * NUM_COLS] = ((uint16_t)(color << 8)) | c;
    col++;

exit:
    return res;
}

KRES klog_init(KRES (*write_func)(char c, log_level_t level))
{
    if (!write_func)
    {
        return -RES_INVARG;
    }

    log_write_func = write_func;
    return RES_SUCCESS;
}

KRES klog_raw(log_level_t level, const char *fmt, ...)
{
    KRES res = RES_SUCCESS;

    va_list args;
    va_start(args, fmt);
    
    char str[KLOG_MAX_LENGTH];
    vsnprintf(str, KLOG_MAX_LENGTH, fmt, args);

    CHECK(klog_write_string(str, level));

exit:
    va_end(args);
    return res;
}

KRES klog(log_level_t level, const char *fmt, ...)
{
    KRES res = RES_SUCCESS;

    va_list args;
    va_start(args, fmt);
    
    char str[KLOG_MAX_LENGTH];
    vsnprintf(str, KLOG_MAX_LENGTH, fmt, args);

    switch (level)
    {
    case LOG_LEVEL_DEBUG:
        CHECK(klog_write_string("DEBUG: ", level));
        break;
    case LOG_LEVEL_INFO:
        CHECK(klog_write_string("INFO:  ", level));
        break;
    case LOG_LEVEL_WARNING:
        CHECK(klog_write_string("WARN:  ", level));
        break;
    case LOG_LEVEL_ERROR:
        CHECK(klog_write_string("ERROR: ", level));
        break;
    }

    CHECK(klog_write_string(str, level));
    CHECK(klog_write_string("\n", level));

exit:
    va_end(args);
    return res;
}

void __kpanic(const char *file, uint64_t line, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    char str[KLOG_MAX_LENGTH];
    vsnprintf(str, KLOG_MAX_LENGTH, msg, args);
    va_end(args);

    klog_raw(LOG_LEVEL_ERROR, "PANIC: The system is unable to continue\n\nInfo:\n - file: %s\n - line: %lld\n\nStack trace:\n", file, line);
    trace_stack(32);
    klog_raw(LOG_LEVEL_ERROR, "\nError message:\n%s\n", str);

    while (1)
    {
        __asm__ volatile("cli");
        __asm__ volatile("hlt");
    }
}
