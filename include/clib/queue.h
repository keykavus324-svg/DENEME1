/**
 * @file queue.h
 * @brief FIFO queue implemented as a ring buffer.
 *
 * Capacity is always a power of two, so advancing an index is a mask (`& (cap-1)`)
 * instead of a modulo. Enqueue and dequeue are amortised O(1); the buffer only
 * moves memory when it doubles.
 */
#ifndef CLIB_QUEUE_H
#define CLIB_QUEUE_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A FIFO queue of borrowed pointers. */
typedef struct clib_queue {
    void **data;        /**< Ring buffer, or NULL when empty. */
    size_t head;        /**< Index of the front element. */
    size_t size;        /**< Number of elements. */
    size_t capacity;    /**< Always a power of two. */
    const clib_allocator *alloc;
} clib_queue;

void clib_queue_init(clib_queue *q, const clib_allocator *a);

/** Append @p value to the back. */
clib_status clib_queue_enqueue(clib_queue *q, void *value);

/** Remove and return the front value, or NULL when empty. */
void *clib_queue_dequeue(clib_queue *q);

/** Front value without removing it, or NULL when empty. */
void *clib_queue_peek(const clib_queue *q);

/** Number of elements. */
size_t clib_queue_size(const clib_queue *q);

/** Release the buffer and reset the struct. */
void clib_queue_free(clib_queue *q);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_QUEUE_H */
