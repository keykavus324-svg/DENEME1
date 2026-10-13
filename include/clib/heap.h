/**
 * @file heap.h
 * @brief Binary min-heap of (priority, value) pairs.
 *
 * The heap is stored as a flat array, so parent/child relations are pure index
 * arithmetic: `parent(i) = (i-1)/2`, `children(i) = 2i+1, 2i+2`.
 *
 * Peek is O(1); push and pop are O(log n). There is no decrease-key, because
 * that needs an external index handle; callers that need it usually want a
 * different structure anyway.
 */
#ifndef CLIB_HEAP_H
#define CLIB_HEAP_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One heap element. */
typedef struct clib_heap_item {
    long long priority;  /**< Lower pops first. */
    void *value;         /**< Borrowed pointer. */
} clib_heap_item;

/** A binary min-heap. */
typedef struct clib_heap {
    clib_heap_item *data;
    size_t size;
    size_t capacity;
    const clib_allocator *alloc;
} clib_heap;

void clib_heap_init(clib_heap *h, const clib_allocator *a);

/** Insert (priority, value). Amortised O(log n). */
clib_status clib_heap_push(clib_heap *h, long long priority, void *value);

/** Highest-priority value (lowest priority number), or NULL when empty. */
void *clib_heap_peek(const clib_heap *h);

/** Remove and return the highest-priority value. O(log n). */
void *clib_heap_pop(clib_heap *h);

/** Number of elements. */
size_t clib_heap_size(const clib_heap *h);

/** Rebuild the heap invariant after priorities were mutated externally. O(n). */
void clib_heap_rebuild(clib_heap *h);

/** Release the buffer and reset the struct. */
void clib_heap_free(clib_heap *h);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_HEAP_H */
