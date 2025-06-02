#ifndef _KERNEL_LOG_H
#define _KERNEL_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/status.h>

typedef enum
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
} log_level_t;

KRES klog_write_e9_and_vga(char c, log_level_t level);
KRES klog_write_e9(char c, log_level_t level);
KRES klog_write_vga(char c, log_level_t level);

KRES klog_init(KRES (*write_func)(char c, log_level_t level));
KRES klog(log_level_t level, const char *fmt, ...);
KRES klog_raw(log_level_t level, const char *fmt, ...);

#define LOG_DEBUG(...) klog(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) klog(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARNING(...) klog(LOG_LEVEL_WARNING, __VA_ARGS__)
#define LOG_ERROR(...) klog(LOG_LEVEL_ERROR, __VA_ARGS__)

void __kpanic(const char *file, uint64_t line, const char *msg, ...);
#define PANIC(...) __kpanic(__FILE__, __LINE__, __VA_ARGS__)
#define PANIC_MINIMAL(msg) klog_raw(LOG_LEVEL_ERROR, "PANIC: The system is unable to continue\n\nError message:\n%s\n", msg); while (1) { __asm__ volatile("cli"); __asm__ volatile("hlt"); }

#endif