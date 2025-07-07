#ifndef _VIDEO_H
#define _VIDEO_H 1

#include <hydra/kernel.h>

#define _SYSCALL_VIDEO_GET_DISPLAY_RECT 9
#define _SYSCALL_VIDEO_CREATE_FRAMEBUFFER 10
#define _SYSCALL_VIDEO_UPDATE_DISPLAY 11

#define FRAMEBUFFER_ADDR_BASE 0x900000

typedef struct
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) video_rect_t;

size_t get_framebuffer_size(video_rect_t *rect);

void get_display_rect(int display_id, video_rect_t *rect);
uint32_t *create_framebuffer(int display_id, video_rect_t *rect);
void update_display(uint32_t *fb, video_rect_t *rect);

#endif