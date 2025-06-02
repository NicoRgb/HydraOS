#ifndef _KERNEL_DBG_H
#define _KERNEL_DBG_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/proc/elf.h>

void trace_stack(uint32_t max_frames);

#endif