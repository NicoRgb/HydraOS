#include <hydra/kernel.h>
#include <hydra/video.h>
#include <canvas/canvas.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

canvas_icon_t load_png_from_file(const char *path);
void free_image(canvas_icon_t *img);

int main(int argc, char **argv)
{
    video_rect_t rect;
    get_display_rect(0, &rect);
    if (rect.width == 0)
    {
        fputs("failed to get display rect\n", stdout);
        return 1;
    }

    uint32_t *fb = create_framebuffer(0, &rect);
    if (!fb)
    {
        fputs("failed to create framebuffer\n", stdout);
        return 1;
    }

    if (!canvas_init(fb, rect.width, rect.height))
    {
        fputs("failed to init canvas\n", stdout);
        return 1;
    }

    // Clear screen to dark gray
    canvas_fill(0xFF202020);

    // Draw rectangles
    canvas_draw_rect(50, 50, 100, 60, 0xFFFF0000);   // Red
    canvas_draw_rect(200, 50, 60, 100, 0xFF00FF00);  // Green

    // Draw circles
    canvas_draw_circle(100, 200, 40, 0xFF0000FF);    // Blue
    canvas_draw_circle(250, 200, 30, 0xFFFFFF00);    // Yellow

    // Draw ellipses
    canvas_draw_ellipse(100, 300, 60, 30, 0xFFFF00FF); // Magenta
    canvas_draw_ellipse(250, 300, 30, 60, 0xFF00FFFF); // Cyan

    canvas_icon_t icon = load_png_from_file("/resources/eye.png");
    if (!icon.pixels)
    {
        fputs("failed to load icon\n", stdout);
        return 1;
    }

    canvas_draw_icon_scaled(250, 50, 256, 256, &icon);

    free_image(&icon);

    update_display(fb, &rect);

    return 0;
}
