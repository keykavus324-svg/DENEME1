/**
 * @file vector.h
 * @brief Growable array with type-safe macros over a single generic core.
 *
 * The `clib_vector(T)` macro declares a struct whose layout matches
 * ::clib_vector_raw, so a typed vector can be handed to the generic
 * implementation through a plain cast. All the growth logic therefore exists
 * exactly once, in src/vector.c.
 *
 * Usage:
 *
 *     clib_vector(int) v;
 *     clib_vector_init(&v);              // or clib_vector_init_with(&v, &alloc)
 *     (void)clib_vector_push(&v, 42);
 *     int x = clib_vector_get(&v, 0);
 *     clib_vector_free(&v);
 */
#ifndef CLIB_VECTOR_H
#define CLIB_VECTOR_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generic, untyped backing struct. Use ::clib_vector instead of declaring this
 * directly; the two must stay layout-compatible.
 */
typedef struct clib_vector_raw {
    void *data;                    /**< Element buffer, may be NULL. */
    size_t size;                   /**< Number of elements in use. */
    size_t capacity;               /**< Elements that fit before regrowing. */
    const clib_allocator *alloc;   /**< Never NULL after init. */
    size_t elem_size;              /**< sizeof(T), recorded by the macros. */
} clib_vector_raw;

/**
 * @brief Declare a vector of @p T.
 * @warning `_elem_size` and `alloc` are implementation details — don't touch.
 */
#define clib_vector(T)                                                        \
    struct {                                                                  \
        T *data;                                                              \
        size_t size;                                                          \
        size_t capacity;                                                      \
        const clib_allocator *alloc;                                          \
        size_t _elem_size;                                                    \
    }

/** Zero-initialise a vector using the default allocator. */
#define clib_vector_init(v)          clib_vector_init_raw((clib_vector_raw *)(v), sizeof(*(v)->data), NULL)
/** Zero-initialise a vector with a custom allocator. */
#define clib_vector_init_with(v, a)  clib_vector_init_raw((clib_vector_raw *)(v), sizeof(*(v)->data), (a))

/** Element at @p i, without bounds checking. */
#define clib_vector_get(v, i)        ((v)->data[i])
/** Number of stored elements. */
#define clib_vector_size(v)          ((v)->size)
/** Elements that fit before the next regrow. */
#define clib_vector_capacity(v)      ((v)->capacity)

/**
 * Append @p val. Returns CLIB_OK, or CLIB_ERR_NOMEM when the buffer cannot be
 * grown — in which case the vector is left unchanged.
 */
#define clib_vector_push(v, val)                                              \
    ( clib_vector_reserve_raw((clib_vector_raw *)(v), 1) == CLIB_OK           \
        ? ((v)->data[(v)->size++] = (val), CLIB_OK)                           \
        : CLIB_ERR_NOMEM )

/** Remove and return the last element. The vector must not be empty. */
#define clib_vector_pop(v)           ((v)->data[--(v)->size])

/** Release the backing buffer and reset all fields to zero. */
#define clib_vector_free(v)          clib_vector_free_raw((clib_vector_raw *)(v))

/* --- generic core, used through the macros above ------------------------- */

clib_status clib_vector_init_raw(clib_vector_raw *v, size_t elem_size,
                                 const clib_allocator *a);

/** Ensure room for @p extra more elements. Capacity grows geometrically. */
clib_status clib_vector_reserve_raw(clib_vector_raw *v, size_t extra);

/** Drop all elements without releasing the buffer. */
void clib_vector_clear(clib_vector_raw *v);

/** Grow or shrink to exactly @p n elements; new elements are zeroed. */
clib_status clib_vector_resize_raw(clib_vector_raw *v, size_t n);

/** Release the buffer and reset the struct. */
void clib_vector_free_raw(clib_vector_raw *v);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_VECTOR_H */
