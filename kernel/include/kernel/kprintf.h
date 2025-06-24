#ifndef _KERNEL_KPRINTF_H
#define _KERNEL_KPRINTF_H

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

#include <kernel/status.h>
#include <kernel/dev/devm.h>

int kprintf_init(device_t *cdev);
void kprintf_free(void);

device_t *kprintf_get_cdev(void);

int kprintf(const char *format, ...);
int vkprintf(const char *format, va_list va);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t count, const char *format, ...);
int vsnprintf(char *buffer, size_t count, const char *format, va_list va);

#endif