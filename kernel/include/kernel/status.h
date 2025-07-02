#ifndef _KERNEL_STATUS_H
#define _KERNEL_STATUS_H

#define KRES int

#define IS_ERROR(r) ((r) < 0)
#define CHECK(r) { KRES _r = r; if IS_ERROR(_r) { res = _r; goto exit; } }

#define RES_SUCCESS 0
#define RES_INVARG 1
#define RES_OVERFLOW 2
#define RES_CORRUPT 4
#define RES_NOMEM 5
#define RES_UNAVAILABLE 6
#define RES_TIMEOUT 7

#define RES_EUNKNOWN 10 // TODO: remove
#define RES_ETEST 11

#endif