#ifndef _STDIO_H
#define _STDIO_H 1

/*
    https://pubs.opengroup.org/onlinepubs/7908799/xsh/stdio.h.html
*/

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef uint64_t FILE;

extern uint64_t _stdin;
extern uint64_t _stdout;
extern uint64_t _stderr;

#define stdin ((FILE *)&_stdin)
#define stdout ((FILE *)&_stdout)
#define stderr ((FILE *)&_stderr)

int fgetc(FILE *f);
char *fgets(char *s, int c, FILE *f);
int fputc(char c, FILE *f);
int fputs(const char *st, FILE *f);
size_t fread(char *s, size_t n, size_t u, FILE *f);
size_t fwrite(const char *st, size_t n, size_t u, FILE *f);
char *gets(char *s);
int putchar(char c);

#define printf printf_
int printf_(const char *st, ...);

#define fprintf fprintf_
int fprintf_(FILE *f, const char *st, ...);

#define vfprintf vfprintf_
int vfprintf_(FILE *f, const char *st, va_list va);

#define snprintf snprintf_
int snprintf_(char *s, size_t n, const char *ct, ...);

#define sprintf sprintf_
int sprintf_(char *s, const char *ct, ...);

#define vprintf vprintf_
int vprintf_(const char *st, va_list va);

#define vsnprintf vsnprintf_
int vsnprintf_(char *s, size_t n, const char *ct, va_list va);

#define vsprintf vsprintf_
int vsprintf_(char *s, const char *ct, va_list va);

#endif