#ifndef _KERNEL_DBG_H
#define _KERNEL_DBG_H

#include <stdint.h>
#include <stddef.h>

void trace_stack(uint32_t max_frames);

#endif