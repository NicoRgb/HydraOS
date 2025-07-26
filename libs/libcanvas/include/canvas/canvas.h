#ifndef _CANVAS_H
#define _CANVAS_H 1

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    uint32_t *framebuffer;
    uint32_t width;
    uint32_t height;
} canvas_context_t;

// state based

canvas_context_t *canvas_init(uint32_t *fb, uint32_t width, uint32_t height);
int canvas_free(canvas_context_t *context);
int canvas_switch(canvas_context_t *context);

void canvas_fill(uint32_t color);
void canvas_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void canvas_draw_circle(int cx, int cy, int radius, uint32_t color);
void canvas_draw_ellipse(int cx, int cy, int rx, int ry, uint32_t color);

typedef struct
{
    int width;
    int height;
    int channels;
    uint32_t* pixels;
} canvas_icon_t;

void canvas_draw_icon(uint32_t x, uint32_t y, canvas_icon_t *icon);
void canvas_draw_icon_scaled(uint32_t x, uint32_t y, uint32_t new_width, uint32_t new_height, canvas_icon_t *icon);

// circles, elypses, triangles, polygones, text, icons (with filters)

#endif