/**
 * @file arena.h
 * @brief Bump allocator: allocate fast, free everything at once.
 *
 * An arena hands out memory by bumping a pointer; there is no per-object free.
 * That makes it a good fit for phases of work whose lifetime is identical —
 * parsing a document, building a compiler IR, serving one request — where
 * individual frees would be pure overhead.
 *
 * ::clib_arena_allocator() adapts an arena to the ::clib_allocator interface, so
 * any clib container can be built on one.
 *
 * @warning The adapter is honest about what it cannot do. `realloc` semantics
 * are only approximated: growing the most recent allocation in place works,
 * anything else allocates a fresh block and copies. Freeing an individual
 * object is a no-op — memory comes back only via clib_arena_reset() or
 * clib_arena_free(). Containers that rely on shrinking will hold on to the
 * larger size until they are destroyed.
 */
#ifndef CLIB_ARENA_H
#define CLIB_ARENA_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One backing block. Blocks are chained; none are returned to the OS early. */
typedef struct clib_arena_block {
    struct clib_arena_block *next;   /**< Previous block, or NULL. */
    size_t used;                     /**< Bytes handed out from this block. */
    size_t cap;                      /**< Bytes available in `data`. */
    /* `data` follows the header in the same allocation. */
} clib_arena_block;

/** A bump allocator. */
typedef struct clib_arena {
    clib_arena_block *cur;       /**< Block currently being bumped. */
    size_t block_size;           /**< Minimum size of a new block. */
    size_t total;                /**< Bytes allocated across all blocks. */
    size_t used;                 /**< Bytes handed out. */
    const clib_allocator *back;  /**< Where blocks come from; NULL = default. */

    /* Bookkeeping that makes the realloc-shaped adapter possible at all. */
    void *last_ptr;              /**< Most recent allocation. */
    size_t last_size;            /**< Size of that allocation. */
} clib_arena;

/** Prepare an empty arena. @p block_size is rounded up internally. */
void clib_arena_init(clib_arena *a, size_t block_size, const clib_allocator *backing);

/** Hand out @p size bytes with at least @p align alignment. NULL on failure. */
void *clib_arena_alloc(clib_arena *a, size_t size, size_t align);

/** Bytes handed out so far. */
size_t clib_arena_used(const clib_arena *a);

/** Bytes obtained from the backing allocator. */
size_t clib_arena_total(const clib_arena *a);

/** Reuse every block without returning them. O(1). */
void clib_arena_reset(clib_arena *a);

/** Return every block to the backing allocator. */
void clib_arena_free(clib_arena *a);

/**
 * Fill @p out with a ::clib_allocator backed by this arena.
 *
 * Typical use:
 *
 *     clib_arena arena;
 *     clib_allocator alloc;
 *     clib_arena_init(&arena, 0, NULL);
 *     clib_arena_allocator(&arena, &alloc);
 *
 *     clib_hashmap m;             // lives on the arena
 *     clib_hashmap_init(&m, &alloc);
 *     ...                         // no individual frees
 *     clib_arena_free(&arena);    // everything goes at once
 */
void clib_arena_allocator(clib_arena *a, clib_allocator *out);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_ARENA_H */
