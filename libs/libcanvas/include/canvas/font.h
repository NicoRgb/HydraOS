#ifndef _FONT_H
#define _FONT_H 1

#include <stdint.h>
#include <stddef.h>

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t glyph_count;
    const uint8_t *glyphs;
} canvas_font_t;

#endif