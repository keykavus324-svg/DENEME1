/**
 * @file hashmap.h
 * @brief Open-addressed hash map from `const char *` keys to `void *` values.
 *
 * Implementation notes:
 *  - **FNV-1a** hashing (fast, no dependency on a good PRNG).
 *  - **Linear probing** with tombstones; capacity is a power of two so the
 *    probe step is a mask.
 *  - Rehashes at a 0.7 load factor, counting tombstones as occupied - otherwise
 *    a map that only ever inserts and deletes would slowly degenerate to O(n).
 *  - Keys are **copied**; values are borrowed.
 */
#ifndef CLIB_HASHMAP_H
#define CLIB_HASHMAP_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Load factor at which the table doubles. */
#define CLIB_HASHMAP_MAX_LOAD 0.70

/** An internal slot. `key == NULL` is empty; a deleted slot is a tombstone. */
typedef struct clib_hashmap_entry {
    char *key;    /**< Owned copy of the key, or NULL / tombstone marker. */
    void *value;  /**< Borrowed pointer. */
} clib_hashmap_entry;

/** Hash map from string keys to borrowed pointers. */
typedef struct clib_hashmap {
    clib_hashmap_entry *slots;
    size_t capacity;    /**< Power of two, 0 before the first insert. */
    size_t size;        /**< Live entries. */
    size_t tombstones;  /**< Deleted slots still being probed over. */
    const clib_allocator *alloc;
} clib_hashmap;

/** Callback for clib_hashmap_foreach(). */
typedef void (*clib_hashmap_visit_fn)(const char *key, void *value, void *ctx);

/** FNV-1a 64-bit hash of a NUL-terminated string. */
size_t clib_hash_str(const char *s);

void clib_hashmap_init(clib_hashmap *m, const clib_allocator *a);

/**
 * Insert or overwrite @p key. The key is **copied** into the map.
 * Returns CLIB_ERR_NOMEM if the table cannot grow or the key cannot be copied.
 */
clib_status clib_hashmap_put(clib_hashmap *m, const char *key, void *value);

/** Look up @p key. Returns the stored value, or NULL when absent. */
void *clib_hashmap_get(const clib_hashmap *m, const char *key);

/** True when @p key is present. */
int clib_hashmap_has(const clib_hashmap *m, const char *key);

/**
 * Pointer to the map's own copy of @p key, or NULL when absent.
 *
 * The returned pointer stays valid until that key is removed or the map is
 * freed. Useful when a second structure wants to reference the key without
 * allocating a duplicate of it (see ::clib_lru).
 */
const char *clib_hashmap_key(const clib_hashmap *m, const char *key);

/** Remove @p key. Returns CLIB_ERR_NOTFOUND when absent. */
clib_status clib_hashmap_remove(clib_hashmap *m, const char *key);

/** Number of live entries. */
size_t clib_hashmap_size(const clib_hashmap *m);

/** Call @p fn for every live entry, in unspecified order. */
void clib_hashmap_foreach(const clib_hashmap *m, clib_hashmap_visit_fn fn, void *ctx);

/** Drop every entry and release the key copies. Values are not freed. */
void clib_hashmap_clear(clib_hashmap *m);

/** Clear and release the slot array. */
void clib_hashmap_free(clib_hashmap *m);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_HASHMAP_H */
