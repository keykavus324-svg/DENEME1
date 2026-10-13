/**
 * @file uf.h
 * @brief Disjoint-set (union-find) over dense indices `0 .. n-1`.
 *
 * Two heuristics make this effectively constant time per operation:
 *
 * - **Union by rank** keeps trees shallow by always hanging the shorter tree
 *   under the taller one.
 * - **Path compression** flattens every node it touches during `find`, so a
 *   second lookup on the same chain is nearly free.
 *
 * Together they give an amortised O(alpha(n)) per operation, where alpha is the
 * inverse Ackermann function — for any input that fits in the universe it is
 * at most 5.
 *
 * The structure also tracks component sizes and how many components remain,
 * which is what you usually want (Kruskal's MST, connected components, image
 * region labelling).
 */
#ifndef CLIB_UF_H
#define CLIB_UF_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A partition of `0 .. n-1` into disjoint sets. */
typedef struct clib_uf {
    size_t *parent;      /**< parent[i], or i itself when i is a root. */
    size_t *rank;        /**< Upper bound on tree height. */
    size_t *set_size;    /**< Only meaningful at roots. */
    size_t n;            /**< Number of elements. */
    size_t sets;         /**< Number of components; n right after init. */
    const clib_allocator *alloc;
} clib_uf;

/** Allocate a partition where every element is its own singleton set. */
clib_status clib_uf_init(clib_uf *u, size_t n, const clib_allocator *a);

/**
 * Representative of the set containing @p x, with path compression applied
 * along the way. Returns `n` when @p x is out of range.
 */
size_t clib_uf_find(clib_uf *u, size_t x);

/**
 * Merge the sets containing @p a and @p b.
 * Returns CLIB_OK, or CLIB_OK-with-nothing-to-do when they already agree;
 * CLIB_ERR_INVAL when either index is out of range.
 */
clib_status clib_uf_union(clib_uf *u, size_t a, size_t b);

/** True when @p a and @p b are in the same set (does not compress). */
int clib_uf_connected(const clib_uf *u, size_t a, size_t b);

/** Size of the set containing @p x; 0 when out of range. */
size_t clib_uf_size_of(const clib_uf *u, size_t x);

/** Number of remaining components. */
size_t clib_uf_sets(const clib_uf *u);

/** Number of elements. */
size_t clib_uf_count(const clib_uf *u);

/** Split every set back into singletons. O(n), keeps the allocation. */
void clib_uf_reset(clib_uf *u);

/** Release all storage. */
void clib_uf_free(clib_uf *u);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_UF_H */
