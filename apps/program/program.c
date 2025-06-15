#include <hydra/kernel.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    fputs("HydraOS Version 0.1 Early Development\nCopyright Nico Grundei 2025\n", stdout);

    for (int i = 0; i < argc; i++)
    {
        printf("%s\n", argv[i]);
    }

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

    return 0;
}
