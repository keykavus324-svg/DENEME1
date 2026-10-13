/**
 * @file stack.h
 * @brief LIFO stack of `void *` backed by a growable array.
 *
 * A contiguous buffer keeps the hot path allocation-free and cache friendly;
 * the trade-off is that pop() does not shrink the buffer (call
 * clib_stack_shrink_to_fit() after draining a large stack).
 */
#ifndef CLIB_STACK_H
#define CLIB_STACK_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A stack of borrowed pointers. */
typedef struct clib_stack {
    void **data;
    size_t size;
    size_t capacity;
    const clib_allocator *alloc;
} clib_stack;

void clib_stack_init(clib_stack *s, const clib_allocator *a);

/** Push @p value. Returns CLIB_ERR_NOMEM if the buffer cannot grow. */
clib_status clib_stack_push(clib_stack *s, void *value);

/** Pop and return the top value, or NULL when empty. */
void *clib_stack_pop(clib_stack *s);

/** Top value without removing it, or NULL when empty. */
void *clib_stack_peek(const clib_stack *s);

/** Number of elements. */
size_t clib_stack_size(const clib_stack *s);

/** Release the buffer and reset the struct. */
void clib_stack_free(clib_stack *s);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_STACK_H */
