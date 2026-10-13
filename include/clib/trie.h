/**
 * @file trie.h
 * @brief Prefix tree mapping NUL-terminated strings to borrowed pointers.
 *
 * Lookup, insert and delete are O(len(key)) regardless of how many keys are
 * stored — that is the whole reason to use a trie instead of a hash map. The
 * trade-off is memory: one node per distinct prefix.
 *
 * Children are kept in a small array **sorted by character**. That costs an
 * O(out-degree) memmove on insert but buys two things: lookups become a binary
 * search, and traversal comes out in lexicographic order for free. For ASCII
 * text the out-degree is at most 26, so both are effectively constant time; a
 * trie over a wider alphabet would swap the array for a hash map.
 *
 * Keys are structural (no copies are made); values are borrowed.
 */
#ifndef CLIB_TRIE_H
#define CLIB_TRIE_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One child edge. */
typedef struct clib_trie_child {
    char ch;                       /**< The character on this edge. */
    struct clib_trie_node *node;   /**< The child, never NULL. */
} clib_trie_child;

/** One node; the root represents the empty prefix. */
typedef struct clib_trie_node {
    clib_trie_child *kids;   /**< Edges to children, sorted by `ch`. */
    size_t nkids;            /**< Edges in use. */
    size_t ckids;            /**< Edges allocated. */
    int terminal;            /**< Non-zero when a key ends here. */
    void *value;             /**< Borrowed; only meaningful when terminal. */
} clib_trie_node;

/** Callback for clib_trie_foreach() and clib_trie_collect(). */
typedef void (*clib_trie_visit_fn)(const char *key, void *value, void *ctx);

/** A trie. */
typedef struct clib_trie {
    clib_trie_node *root;
    size_t size;                 /**< Number of terminal keys. */
    size_t nodes;                /**< Number of nodes, for diagnostics. */
    const clib_allocator *alloc;
} clib_trie;

/** Create an empty trie (root only). */
clib_status clib_trie_init(clib_trie *t, const clib_allocator *a);

/**
 * Insert or overwrite @p key. The key is **not** copied by the trie — it is
 * stored structurally — but the caller must keep it alive only until the next
 * operation; lookups compare against the query string, not a stored buffer.
 */
clib_status clib_trie_put(clib_trie *t, const char *key, void *value);

/** Look up @p key. Returns the value, or NULL when absent. */
void *clib_trie_get(const clib_trie *t, const char *key);

/** True when @p key is present. */
int clib_trie_has(const clib_trie *t, const char *key);

/**
 * Remove @p key and return its value, or NULL when absent.
 * Nodes that become childless and non-terminal are pruned, so repeated
 * insert/delete cycles do not leak nodes.
 */
void *clib_trie_remove(clib_trie *t, const char *key);

/** Number of stored keys. */
size_t clib_trie_size(const clib_trie *t);

/**
 * Call @p fn for every key starting with @p prefix, in lexicographic order.
 * Pass "" (or NULL) to enumerate everything. Returns how many keys matched.
 */
size_t clib_trie_collect(const clib_trie *t, const char *prefix,
                         clib_trie_visit_fn fn, void *ctx);

/** Remove every key and free all nodes except the root. */
void clib_trie_clear(clib_trie *t);

/** Free the whole tree, root included. */
void clib_trie_free(clib_trie *t);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_TRIE_H */
