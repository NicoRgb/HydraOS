#include <hydra/video.h>

#define PAGE_SIZE 4096

int64_t syscall_video_get_display_rect(int display_id, video_rect_t *rect)
{
    return syscall(_SYSCALL_VIDEO_GET_DISPLAY_RECT, (uint64_t)display_id, (uint64_t)rect, 0, 0, 0, 0);
}

int64_t syscall_video_create_framebuffer(int display_id, video_rect_t *rect, uint32_t *fb_addr)
{
    return syscall(_SYSCALL_VIDEO_CREATE_FRAMEBUFFER, (uint64_t)display_id, (uint64_t)rect, (uint64_t)fb_addr, 0, 0, 0);
}

int64_t syscall_video_update_display(uint32_t *fb, video_rect_t *rect)
{
    return syscall(_SYSCALL_VIDEO_UPDATE_DISPLAY, (uint64_t)fb, (uint64_t)rect, 0, 0, 0, 0);
}

size_t get_framebuffer_size(video_rect_t *rect)
{
    return rect->width * rect->height * 4;
}

static void *page_align_address_higer(void *addr)
{
    uintptr_t _addr = (uintptr_t)addr;
    _addr += PAGE_SIZE - (_addr % PAGE_SIZE);
    return (void *)_addr;
}

static uintptr_t current_framebuffer_base = FRAMEBUFFER_ADDR_BASE;

static uint32_t *new_framebuffer_addr(video_rect_t *rect)
{
    size_t size = get_framebuffer_size(rect);
    uintptr_t res = current_framebuffer_base;
    current_framebuffer_base += size;
    current_framebuffer_base = (uintptr_t)page_align_address_higer((void *)current_framebuffer_base);

    return (uint32_t *)res;
}

void get_display_rect(int display_id, video_rect_t *rect)
{
    if (syscall_video_get_display_rect(display_id, rect) < 0)
    {
        rect->width = 0;
        rect->height = 0;
        rect->x = 0;
        rect->y = 0;
    }
}

uint32_t *create_framebuffer(int display_id, video_rect_t *rect)
{
    uint32_t *fb = new_framebuffer_addr(rect);

    if (syscall_video_create_framebuffer(display_id, rect, fb) < 0)
    {
        return NULL;
    }

    return fb;
}

void update_display(uint32_t *fb, video_rect_t *rect)
{
    syscall_video_update_display(fb, rect);
}
