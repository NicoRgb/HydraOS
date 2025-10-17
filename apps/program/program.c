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

    canvas_fill(0xFF202020);

    canvas_icon_t icon = load_png_from_file("/resources/logo-small.png");
    if (!icon.pixels)
    {
        fputs("failed to load icon\n", stdout);
        return 1;
    }

    canvas_draw_icon_scaled(0, 0, 512, 512, &icon);

    free_image(&icon);

    update_display(fb, &rect);

    return 0;
}
