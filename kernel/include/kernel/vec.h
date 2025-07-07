#ifndef _KERNEL_VEC_H
#define _KERNEL_VEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <kernel/status.h>
#include <kernel/kmm.h>

#define CVECTOR NULL

#define cvector(type) type *

#define _VEC_CAPACITY_INCREASAE 8

typedef struct
{
    size_t size;
    size_t capacity;
} vector_metadata_t;

#define _vec_get_metadata(vec) \
    (&(((vector_metadata_t *)vec)[-1]))

#define cvector_init(vec_ptr)                                                                          \
    {                                                                                                  \
        vec_ptr = kmalloc(sizeof(vec_ptr[0]) * _VEC_CAPACITY_INCREASAE + sizeof(vector_metadata_t *)); \
        vec_ptr += sizeof(vector_metadata_t *);                                                        \
        memset(vec_ptr, 0, sizeof(sizeof((vec_ptr)[0]) * _VEC_CAPACITY_INCREASAE));                    \
        vector_metadata_t *_vec_metadata = _vec_get_metadata(vec_ptr);                                 \
        _vec_metadata->size = 0;                                                                       \
        _vec_metadata->capacity = _VEC_CAPACITY_INCREASAE;                                             \
    }

#define cvector_push(vec_ptr, elem)                                                                                                      \
    {                                                                                                                                    \
        if (vec_ptr == CVECTOR)                                                                                                          \
        {                                                                                                                                \
            cvector_init(vec_ptr);                                                                                                       \
        }                                                                                                                                \
                                                                                                                                         \
        vector_metadata_t *_vec_metadata = _vec_get_metadata(vec_ptr);                                                                   \
        if (_vec_metadata->capacity < _vec_metadata->size + 1)                                                                           \
        {                                                                                                                                \
            _vec_metadata->capacity += _VEC_CAPACITY_INCREASAE;                                                                          \
            vec_ptr = krealloc(vec_ptr - sizeof(vector_metadata_t *),                                                                    \
                               sizeof((vec_ptr)[0]) * (_vec_metadata->capacity - _VEC_CAPACITY_INCREASAE) + sizeof(vector_metadata_t *), \
                               sizeof((vec_ptr)[0]) * _vec_metadata->capacity + sizeof(vector_metadata_t *));                            \
            _vec_metadata = _vec_get_metadata(vec_ptr);                                                                                  \
        }                                                                                                                                \
                                                                                                                                         \
        vec_ptr[_vec_metadata->size++] = elem;                                                                                           \
    }

#define cvector_size(vec_ptr) \
    (_vec_get_metadata(vec_ptr)->size)

#endif