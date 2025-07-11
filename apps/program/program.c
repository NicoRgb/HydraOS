#include <hydra/kernel.h>
#include <hydra/video.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    fputs("HydraOS Version 0.1 Early Development\nCopyright Nico Grundei 2025\n", stdout);

    for (int i = 0; i < argc; i++)
    {
        printf("%s\n", argv[i]);
    }

    FILE *cdev = fopen("9:/char0", "r");
    if (!cdev)
    {
        fputs("failed to open char device\n", stdout);
        return 1;
    }

    fputs("test123\n", cdev);
    fclose(cdev);

    FILE *fp = fopen("0:/test.txt", "c");
    if (!fp)
    {
        fputs("failed to open file\n", stdout);
        return 1;
    }

    fputs("This is a test txt\n", fp);

    fclose(fp);

    fp = fopen("0:/test.txt", "r");
    if (!fp)
    {
        fputs("failed to open file\n", stdout);
        return 1;
    }

    char str[20];
    fread(str, 19, 1, fp);
    str[19] = 0;

    fputs(str, stdout);

    fclose(fp);

    video_rect_t rect;
    get_display_rect(0, &rect);
    if (rect.width == 0)
    {
        fputs("failed to get display rect\n", stdout);
        return 1;
    }

    printf("width %lld, height %lld\n", rect.width, rect.height);

    uint32_t *fb = create_framebuffer(0, &rect);
    if (!fb)
    {
        fputs("failed to create framebuffer\n", stdout);
        return 1;
    }

    size_t fb_size = get_framebuffer_size(&rect);
    memset((void *)fb, 0xFFFFFFFF, fb_size);

    update_display(fb, &rect);

    return 0;
}
