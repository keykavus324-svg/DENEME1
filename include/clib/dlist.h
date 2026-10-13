/**
 * @file dlist.h
 * @brief Doubly linked list with O(1) insert/remove given a node.
 *
 * Unlike ::clib_slist, a node can be unlinked in O(1) without knowing its
 * predecessor, which is what makes this the right backing store for LRU
 * caches and similar structures.
 */
#ifndef CLIB_DLIST_H
#define CLIB_DLIST_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One element of a ::clib_dlist. */
typedef struct clib_dlist_node {
    void *value;                  /**< Stored pointer, owned by the caller. */
    struct clib_dlist_node *prev; /**< Previous node, or NULL. */
    struct clib_dlist_node *next; /**< Next node, or NULL. */
} clib_dlist_node;

/** A doubly linked list. */
typedef struct clib_dlist {
    clib_dlist_node *head;
    clib_dlist_node *tail;
    size_t size;
    const clib_allocator *alloc;
} clib_dlist;

/** Callback invoked by clib_dlist_foreach(). */
typedef void (*clib_dlist_visit_fn)(void *value, void *ctx);

void clib_dlist_init(clib_dlist *l, const clib_allocator *a);

clib_status clib_dlist_push_front(clib_dlist *l, void *value);
clib_status clib_dlist_push_back(clib_dlist *l, void *value);

void *clib_dlist_pop_front(clib_dlist *l);
void *clib_dlist_pop_back(clib_dlist *l);

void *clib_dlist_front(const clib_dlist *l);
void *clib_dlist_back(const clib_dlist *l);

/** Insert @p value immediately before @p pos. @p pos NULL means push_back. */
clib_status clib_dlist_insert_before(clib_dlist *l, clib_dlist_node *pos, void *value);

/** Unlink @p n in O(1). The caller owns the value; @p n itself is freed. */
void clib_dlist_erase(clib_dlist *l, clib_dlist_node *n);

/** Move @p n to the front - the building block of an LRU cache. */
void clib_dlist_move_to_front(clib_dlist *l, clib_dlist_node *n);

void clib_dlist_foreach(const clib_dlist *l, clib_dlist_visit_fn fn, void *ctx);
void clib_dlist_clear(clib_dlist *l);
void clib_dlist_free(clib_dlist *l);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_DLIST_H */
