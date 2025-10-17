#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG // Disable all formats except PNG
#define STBI_NO_STDIO // We'll use memory loading, not fopen
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_TGA
#define STBI_NO_BMP
#define STBI_NO_JPEG

#define STBI_ASSERT \
    {               \
    }

#include "stb_image.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <canvas/canvas.h>

canvas_icon_t load_png_from_file(const char *path)
{
    canvas_icon_t result;
    memset(&result, 0, sizeof(canvas_icon_t));

    FILE *f = fopen(path, "rb");
    if (!f)
        return result;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buffer = (uint8_t *)malloc(size);
    if (!buffer)
    {
        fclose(f);
        return result;
    }

    printf("size: %ld, top: %p\n", size, (uint8_t *)((uintptr_t)buffer + size));

    fread(buffer, 1, size, f);
    fclose(f);

    int w, h, channels;
    uint32_t *data = (uint32_t *)stbi_load_from_memory(buffer, size, &w, &h, &channels, 4); // force RGBA
    free(buffer);

    if (data)
    {
        result.width = w;
        result.height = h;
        result.channels = 4;
        result.pixels = data;
    }

    return result;
}

void free_image(canvas_icon_t *img)
{
    if (img->pixels)
    {
        stbi_image_free(img->pixels);
        img->pixels = NULL;
    }
}
