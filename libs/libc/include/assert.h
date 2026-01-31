#ifndef _ASSERT_H
#define _ASSERT_H 1

#include <stdio.h>

void abort(void); // NOTE: this is not posix compliant

#define assert(expr) do { if (!expr) { fprintf(stderr, "%s:%lld: assertion `%s` failed\n", __FILE_NAME__, __LINE__, #expr); abort(); } } while(0)

#endif