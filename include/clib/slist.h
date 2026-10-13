/**
 * @file slist.h
 * @brief Singly linked list storing `void *` values.
 *
 * Nodes are heap allocated one by one, so insertions never invalidate
 * pointers to existing elements. `head` and `tail` are both maintained, which
 * makes push_back O(1) at the cost of one extra pointer per list.
 */
#ifndef CLIB_SLIST_H
#define CLIB_SLIST_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One element of a ::clib_slist. */
typedef struct clib_slist_node {
    void *value;                 /**< Stored pointer, owned by the caller. */
    struct clib_slist_node *next;/**< Next node, or NULL. */
} clib_slist_node;

/** A singly linked list. Zero-initialise or call clib_slist_init(). */
typedef struct clib_slist {
    clib_slist_node *head;       /**< First node, or NULL when empty. */
    clib_slist_node *tail;       /**< Last node, or NULL when empty. */
    size_t size;                 /**< Number of nodes. */
    const clib_allocator *alloc; /**< NULL means the default allocator. */
} clib_slist;

/** Callback invoked by clib_slist_foreach(). */
typedef void (*clib_slist_visit_fn)(void *value, void *ctx);

/** Prepare @p l for use. @p a may be NULL. */
void clib_slist_init(clib_slist *l, const clib_allocator *a);

/** Insert at the front. Returns CLIB_ERR_NOMEM if the node cannot be allocated. */
clib_status clib_slist_push_front(clib_slist *l, void *value);

/** Insert at the back. O(1) thanks to the maintained tail pointer. */
clib_status clib_slist_push_back(clib_slist *l, void *value);

/** Remove and return the front value, or NULL when empty. */
void *clib_slist_pop_front(clib_slist *l);

/** Front value, or NULL when empty. */
void *clib_slist_front(const clib_slist *l);

/** First node whose value equals @p value, or NULL. */
clib_slist_node *clib_slist_find(const clib_slist *l, const void *value);

/** Remove the first node holding @p value. Returns CLIB_ERR_NOTFOUND if absent. */
clib_status clib_slist_remove(clib_slist *l, const void *value);

/** Reverse the links in place. O(n), no allocation. */
void clib_slist_reverse(clib_slist *l);

/** Call @p fn for every value, in order. */
void clib_slist_foreach(const clib_slist *l, clib_slist_visit_fn fn, void *ctx);

/** Remove every node. Values are *not* freed - the caller owns them. */
void clib_slist_clear(clib_slist *l);

/** Alias for clib_slist_clear(); the list owns no values. */
void clib_slist_free(clib_slist *l);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_SLIST_H */
