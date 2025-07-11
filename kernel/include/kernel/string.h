#ifndef _KERNEL_STRING_H
#define _KERNEL_STRING_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/status.h>

size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *to, const char *from);
char *strcat(char *s, const char *ct);

void *memset(void *dest, register int val, register size_t len);
void memcpy(void *dest, const void *src, size_t len);
int memcmp(const char *cs_in, const char *ct_in, size_t n);

int atoi(char *s);

int isdigit(char c);
int isspace(char c);
int islower(char c);
int isupper(char c);
int isalpha(char c);

long strtol(const char *restrict nptr, char **restrict endptr, int base);
char *strchr(const char *p, int ch);

void *memrchr(const void *s, int c, size_t n);
char *strtok(char *str, char *delim);
char *strrchr(const char *str, int ch);
int tolower(int c);
char *strdup(const char *s1);

int toupper(int c);
int isalnum(int c);
char *strncpy(char *dest, const char *src, size_t n);

#endif