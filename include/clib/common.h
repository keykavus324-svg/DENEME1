/**
 * @file common.h
 * @brief Shared status codes, allocator interface and small helpers.
 *
 * Every container in clib accepts a ::clib_allocator so that callers can plug
 * in an arena, a pool, or any custom memory source. Passing NULL selects the
 * default `malloc`/`free` pair.
 */
#ifndef CLIB_COMMON_H
#define CLIB_COMMON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result of an operation that may fail. */
typedef enum clib_status {
    CLIB_OK = 0,            /**< Success. */
    CLIB_ERR_NOMEM = -1,    /**< Allocation failed. */
    CLIB_ERR_FULL = -2,     /**< Container is at capacity. */
    CLIB_ERR_EMPTY = -3,    /**< Container has nothing to return. */
    CLIB_ERR_NOTFOUND = -4, /**< Key/index not present. */
    CLIB_ERR_INVAL = -5     /**< Argument is invalid. */
} clib_status;

/** Human-readable, static string for a status code. Never returns NULL. */
const char *clib_status_string(clib_status s);

/**
 * @brief Allocation callbacks.
 *
 * @p realloc_fn must behave like `realloc`: it frees the old block when the
 * request cannot be satisfied, and it must accept a NULL pointer (then it acts
 * like `malloc`) and a zero size (then it frees and returns NULL).
 */
typedef struct clib_allocator {
    void *(*alloc_fn)(void *ptr, size_t size, void *ctx);
    void *ctx;
} clib_allocator;

/** The process-wide default allocator (malloc/free). */
extern clib_allocator clib_default_allocator;

/** Allocate @p size bytes, or NULL on failure. */
void *clib_alloc(const clib_allocator *a, size_t size);

/** Resize a block previously returned by clib_alloc(). */
void *clib_realloc(const clib_allocator *a, void *ptr, size_t size);

/** Release a block previously returned by clib_alloc()/clib_realloc(). */
void clib_free(const clib_allocator *a, void *ptr);

/** Container-independent three-way comparison result convention. */
typedef int (*clib_cmp_fn)(const void *lhs, const void *rhs, void *ctx);

/** Comparison function suitable for `int` keys stored by value. */
int clib_cmp_int(const void *lhs, const void *rhs, void *ctx);

/** Comparison function for NUL-terminated `const char *` keys. */
int clib_cmp_str(const void *lhs, const void *rhs, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_COMMON_H */
