/**
 * @file lru.h
 * @brief Fixed-capacity LRU cache: hash map for lookup, list for recency.
 *
 * This is the module where the rest of the library pays off. A hash map gives
 * O(1) lookup by key; a doubly linked list gives O(1) reordering and O(1)
 * eviction. Neither can do the whole job alone.
 *
 * - `clib_lru_get()` is O(1) and **counts as a use**, bumping the key to most
 *   recently used — that is what makes it LRU and not just FIFO.
 * - At capacity, `clib_lru_put()` evicts the least recently used entry.
 *
 * Keys are copied; values are borrowed. Pass ::clib_lru_free_fn if you want the
 * cache to release values on eviction.
 */
#ifndef CLIB_LRU_H
#define CLIB_LRU_H

#include <clib/common.h>
#include <clib/dlist.h>
#include <clib/hashmap.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Called with a value that is being evicted or dropped. May be NULL. */
typedef void (*clib_lru_free_fn)(const char *key, void *value, void *ctx);

/** One cached pair. The key points into the map's own copy - do not free it. */
typedef struct clib_lru_entry {
    const char *key;   /**< Borrowed from the internal hash map. */
    void *value;       /**< Borrowed from the caller. */
} clib_lru_entry;

/** An LRU cache. */
typedef struct clib_lru {
    clib_hashmap map;       /**< key -> owning clib_dlist_node *. */
    clib_dlist order;       /**< front = most recently used. */
    size_t capacity;        /**< Maximum live entries; 0 means unbounded. */
    clib_lru_free_fn on_evict;
    void *on_evict_ctx;
    const clib_allocator *alloc;
} clib_lru;

/** Prepare a cache holding at most @p capacity entries. */
void clib_lru_init(clib_lru *c, size_t capacity, clib_lru_free_fn on_evict,
                   void *ctx, const clib_allocator *a);

/**
 * Look up @p key. Returns the value, or NULL when absent.
 * On a hit the entry becomes the most recently used one.
 */
void *clib_lru_get(clib_lru *c, const char *key);

/** True when @p key is present, without changing recency. */
int clib_lru_has(const clib_lru *c, const char *key);

/**
 * Insert or replace @p key. The key is copied, the value is borrowed.
 * Returns CLIB_ERR_NOMEM if the entry cannot be allocated.
 */
clib_status clib_lru_put(clib_lru *c, const char *key, void *value);

/** Remove @p key, calling the eviction callback on its value. */
clib_status clib_lru_remove(clib_lru *c, const char *key);

/** Number of live entries. */
size_t clib_lru_size(const clib_lru *c);

/** Configured maximum; 0 means unbounded. */
size_t clib_lru_capacity(const clib_lru *c);

/** Drop every entry, invoking the eviction callback for each value. */
void clib_lru_clear(clib_lru *c);

/** Clear and release all internal storage. */
void clib_lru_free(clib_lru *c);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_LRU_H */
