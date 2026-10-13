#include <clib/arena.h>
#include <stddef.h>
#include <stdalign.h>
#include <string.h>

#define CLIB_ARENA_MIN_BLOCK 1024u
#define CLIB_ARENA_HEADER    (sizeof(clib_arena_block))

static size_t align_up(size_t n, size_t a)
{
    size_t r = n % a;
    return r ? n + (a - r) : n;
}

static void *block_data(clib_arena_block *b)
{
    return (void *)((unsigned char *)b + CLIB_ARENA_HEADER);
}

void clib_arena_init(clib_arena *a, size_t block_size, const clib_allocator *backing)
{
    if (!a) return;
    a->cur = NULL;
    a->block_size = block_size < CLIB_ARENA_MIN_BLOCK ? CLIB_ARENA_MIN_BLOCK : block_size;
    a->total = 0;
    a->used = 0;
    a->back = backing;
    a->last_ptr = NULL;
    a->last_size = 0;
}

static clib_arena_block *block_new(clib_arena *a, size_t need)
{
    size_t cap = need > a->block_size ? need : a->block_size;
    clib_arena_block *b;

    b = clib_alloc(a->back, CLIB_ARENA_HEADER + cap);
    if (!b) return NULL;

    b->next = a->cur;
    b->used = 0;
    b->cap = cap;
    a->cur = b;
    a->total += CLIB_ARENA_HEADER + cap;
    return b;
}

void *clib_arena_alloc(clib_arena *a, size_t size, size_t align)
{
    clib_arena_block *b;
    size_t offset, base;

    if (!a || size == 0) return NULL;
    if (align == 0) align = alignof(max_align_t);

    /* Keep to power-of-two alignment so the mask arithmetic stays valid. */
    if (align & (align - 1)) return NULL;

    for (;;) {
        unsigned char *p;

        if (!a->cur) {
            if (!block_new(a, size + align)) return NULL;
        }
        b = a->cur;

        base = (size_t)block_data(b) + b->used;
        offset = align_up(base, align) - base;
        if (offset > b->cap - b->used || size > b->cap - b->used - offset) {
            if (!block_new(a, size + align)) return NULL;
            continue;
        }

        p = (unsigned char *)block_data(b) + b->used + offset;
        b->used += offset + size;
        a->used += size;

        a->last_ptr = p;
        a->last_size = size;
        return p;
    }
}

size_t clib_arena_used(const clib_arena *a)
{
    return a ? a->used : 0;
}

size_t clib_arena_total(const clib_arena *a)
{
    return a ? a->total : 0;
}

void clib_arena_reset(clib_arena *a)
{
    clib_arena_block *b;

    if (!a) return;
    for (b = a->cur; b; b = b->next) b->used = 0;

    a->used = 0;
    a->last_ptr = NULL;
    a->last_size = 0;
}

void clib_arena_free(clib_arena *a)
{
    clib_arena_block *b, *next;

    if (!a) return;
    for (b = a->cur; b; b = next) {
        next = b->next;
        clib_free(a->back, b);
    }
    a->cur = NULL;
    a->total = a->used = 0;
    a->last_ptr = NULL;
    a->last_size = 0;
}

/*
 * The clib_allocator contract is realloc-shaped:
 *   ptr == NULL, size > 0   -> allocate
 *   ptr != NULL, size > 0   -> resize
 *   ptr != NULL, size == 0  -> free
 *
 * An arena can do the first one natively. The other two it can only approximate,
 * and it is better to say so than to pretend.
 */
static void *arena_realloc(void *ptr, size_t size, void *ctx)
{
    clib_arena *a = ctx;

    if (!a) return NULL;

    /* Individual frees are impossible; report success and keep the memory. */
    if (size == 0) return NULL;

    if (!ptr) return clib_arena_alloc(a, size, 0);

    if (ptr == a->last_ptr) {
        size_t old = a->last_size;      /* read BEFORE the next alloc clobbers it */

        if (size <= old) {                          /* shrink: just re-label */
            a->last_size = size;
            return ptr;
        }

        /* Grow in place, but only if nothing has been handed out since. */
        {
            unsigned char *end = (unsigned char *)block_data(a->cur) + a->cur->used;
            if ((unsigned char *)ptr + old == end &&
                size - old <= a->cur->cap - a->cur->used) {
                a->cur->used += size - old;
                a->used += size - old;
                a->last_size = size;
                return ptr;
            }
        }

        /* Otherwise allocate a new run and copy the bytes we know the size of. */
        {
            void *fresh = clib_arena_alloc(a, size, 0);
            if (!fresh) return NULL;
            memcpy(fresh, ptr, old);
            return fresh;
        }
    }

    /*
     * An older allocation: the arena does not record per-object sizes, so there
     * is nothing safe to copy. Hand out fresh memory and leave the old bytes
     * behind - wasted arena space, but never a buffer overrun.
     */
    return clib_arena_alloc(a, size, 0);
}

void clib_arena_allocator(clib_arena *a, clib_allocator *out)
{
    if (!out) return;
    out->alloc_fn = arena_realloc;
    out->ctx = a;
}
