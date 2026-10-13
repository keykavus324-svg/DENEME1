/**
 * @file bst.h
 * @brief Unbalanced binary search tree keyed by `const char *`.
 *
 * Insert, lookup and deletion are O(h) where h is the tree height. Without
 * rebalancing a sorted insert order degrades the tree to a list, so this is
 * meant for small maps and for teaching - use ::clib_hashmap when you need
 * predictable lookups, or a self-balancing tree for ordered iteration.
 *
 * Keys are borrowed (not copied): the caller must keep them alive.
 */
#ifndef CLIB_BST_H
#define CLIB_BST_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Callback for clib_bst_foreach(); called with keys in ascending order. */
typedef void (*clib_bst_visit_fn)(const char *key, void *value, void *ctx);

/** A search tree of borrowed key/value pairs. */
typedef struct clib_bst {
    struct clib_bst_node *root;  /**< NULL when empty. */
    size_t size;
    const clib_allocator *alloc;
} clib_bst;

void clib_bst_init(clib_bst *t, const clib_allocator *a);

/** Insert or overwrite @p key (borrowed, not copied). */
clib_status clib_bst_put(clib_bst *t, const char *key, void *value);

/** Look up @p key; returns the value, or NULL when absent. */
void *clib_bst_get(const clib_bst *t, const char *key);

/** Remove @p key. Returns CLIB_ERR_NOTFOUND when absent. */
clib_status clib_bst_remove(clib_bst *t, const char *key);

/** Number of nodes. */
size_t clib_bst_size(const clib_bst *t);

/** Node count along the longest root-to-leaf path; 0 when empty. */
size_t clib_bst_height(const clib_bst *t);

/** In-order walk, so keys arrive sorted. */
void clib_bst_foreach(const clib_bst *t, clib_bst_visit_fn fn, void *ctx);

/** Drop every node. Keys and values are not freed - they are borrowed. */
void clib_bst_clear(clib_bst *t);

void clib_bst_free(clib_bst *t);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_BST_H */
