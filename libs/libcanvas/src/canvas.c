#include <canvas/canvas.h>
#include <stdlib.h>
#include <string.h>

static canvas_context_t *current_context = NULL;

canvas_context_t *canvas_init(uint32_t *fb, uint32_t width, uint32_t height)
{
    canvas_context_t *res = malloc(sizeof(canvas_context_t));
    if (!res)
    {
        return NULL;
    }

    res->framebuffer = fb;
    res->width = width;
    res->height = height;

    current_context = res;

    return res;
}

int canvas_free(canvas_context_t *context)
{
    if (!context)
    {
        return -1;
    }

    if (current_context == context)
    {
        current_context = NULL;
    }

    free(context);
    return 0;
}

int canvas_switch(canvas_context_t *context)
{
    current_context = context;
    return 0;
}

static inline void put_pixel_safe(int x, int y, uint32_t color)
{
    if (!current_context)
    {
        return;
    }
    if (x < 0 || y < 0 || x >= (int)current_context->width || y >= (int)current_context->height)
    {
        return;
    }

    uint8_t alpha = (color >> 24) & 0xFF;
    if (alpha == 0)
    {
        return;
    }

    if (alpha == 255)
    {
        current_context->framebuffer[y * current_context->width + x] = color;
        return;
    }

    uint32_t dst = current_context->framebuffer[y * current_context->width + x];

    uint8_t src_r = (color >> 16) & 0xFF;
    uint8_t src_g = (color >> 8) & 0xFF;
    uint8_t src_b = color & 0xFF;

    uint8_t dst_r = (dst >> 16) & 0xFF;
    uint8_t dst_g = (dst >> 8) & 0xFF;
    uint8_t dst_b = dst & 0xFF;

    uint8_t out_r = (src_r * alpha + dst_r * (255 - alpha)) / 255;
    uint8_t out_g = (src_g * alpha + dst_g * (255 - alpha)) / 255;
    uint8_t out_b = (src_b * alpha + dst_b * (255 - alpha)) / 255;

    current_context->framebuffer[y * current_context->width + x] =
        (0xFF << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

void canvas_fill(uint32_t color)
{
    for (uint32_t row = 0; row < current_context->height; row++)
    {
        for (uint32_t col = 0; col < current_context->width; col++)
        {
            put_pixel_safe(col, row, color);
        }
    }
}

void canvas_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    for (uint32_t row = y; row < y + height; row++)
    {
        for (uint32_t col = x; col < x + width; col++)
        {
            put_pixel_safe(col, row, color);
        }
    }
}

void canvas_draw_circle(int cx, int cy, int radius, uint32_t color)
{
    int x = 0;
    int y = radius;
    int d = 1 - radius;

    while (y >= x)
    {
        put_pixel_safe(cx + x, cy + y, color);
        put_pixel_safe(cx - x, cy + y, color);
        put_pixel_safe(cx + x, cy - y, color);
        put_pixel_safe(cx - x, cy - y, color);
        put_pixel_safe(cx + y, cy + x, color);
        put_pixel_safe(cx - y, cy + x, color);
        put_pixel_safe(cx + y, cy - x, color);
        put_pixel_safe(cx - y, cy - x, color);

        x++;
        if (d < 0)
        {
            d += 2 * x + 1;
        }
        else
        {
            y--;
            d += 2 * (x - y) + 1;
        }
    }
}

void canvas_draw_ellipse(int cx, int cy, int rx, int ry, uint32_t color)
{
    int x = 0, y = ry;
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int dx = 2 * ry2 * x;
    int dy = 2 * rx2 * y;
    int d1 = ry2 - (rx2 * ry) + (0.25 * rx2);

    while (dx < dy)
    {
        put_pixel_safe(cx + x, cy + y, color);
        put_pixel_safe(cx - x, cy + y, color);
        put_pixel_safe(cx + x, cy - y, color);
        put_pixel_safe(cx - x, cy - y, color);

        x++;
        dx += 2 * ry2;
        if (d1 < 0)
        {
            d1 += dx + ry2;
        }
        else
        {
            y--;
            dy -= 2 * rx2;
            d1 += dx - dy + ry2;
        }
    }

    int d2 = ry2 * (x + 0.5f) * (x + 0.5f) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
    while (y >= 0)
    {
        put_pixel_safe(cx + x, cy + y, color);
        put_pixel_safe(cx - x, cy + y, color);
        put_pixel_safe(cx + x, cy - y, color);
        put_pixel_safe(cx - x, cy - y, color);

        y--;
        dy -= 2 * rx2;
        if (d2 > 0)
        {
            d2 += rx2 - dy;
        }
        else
        {
            x++;
            dx += 2 * ry2;
            d2 += dx - dy + rx2;
        }
    }
}

void canvas_draw_icon(uint32_t x, uint32_t y, canvas_icon_t *icon)
{
    for (uint32_t row = 0; row < icon->height; row++)
    {
        for (uint32_t col = 0; col < icon->width; col++)
        {
            put_pixel_safe(col + x, row + y, icon->pixels[row * icon->width + col]);
        }
    }
}

void canvas_draw_icon_scaled(uint32_t x, uint32_t y, uint32_t new_width, uint32_t new_height, canvas_icon_t *icon)
{
    for (uint32_t dst_row = 0; dst_row < new_height; dst_row++)
    {
        uint32_t src_row = (dst_row * icon->height) / new_height;

        for (uint32_t dst_col = 0; dst_col < new_width; dst_col++)
        {
            uint32_t src_col = (dst_col * icon->width) / new_width;

            uint32_t color = icon->pixels[src_row * icon->width + src_col];
            put_pixel_safe(x + dst_col, y + dst_row, color);
        }
    }
}
