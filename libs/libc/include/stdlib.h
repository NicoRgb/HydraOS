#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <stddef.h>

void *malloc(size_t s);
void free(void *p);
void *realloc(void *p, size_t s);

char *getenv(const char *name);
//int setenv(const char *name, const char *value, int overwrite);
//int unsetenv(const char *name);
//int putenv(char *string);

#endif