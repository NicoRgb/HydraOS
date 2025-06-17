#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*extern uint8_t alignment;

typedef struct buddy_header
{
    size_t size;
    uint8_t flags;
    struct buddy_header *next;
    struct buddy_header *prev;
} buddy_header_t;*/

void *realloc(void *p, size_t s)
{
    (void)p;
    (void)s;
    //buddy_header_t *buddy = (buddy_header_t *)((uintptr_t)p - alignment);

    //void *res = malloc(s);
    //memcpy(res, p, buddy->size);
    //free(p);
    //return res;
    return NULL;
}
