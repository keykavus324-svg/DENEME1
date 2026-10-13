#include <clib/slist.h>

void clib_slist_init(clib_slist *l, const clib_allocator *a)
{
    if (!l) return;
    l->head = l->tail = NULL;
    l->size = 0;
    l->alloc = a;
}

clib_status clib_slist_push_front(clib_slist *l, void *value)
{
    clib_slist_node *n;

    if (!l) return CLIB_ERR_INVAL;

    n = clib_alloc(l->alloc, sizeof *n);
    if (!n) return CLIB_ERR_NOMEM;

    n->value = value;
    n->next = l->head;
    l->head = n;
    if (!l->tail) l->tail = n;
    l->size++;
    return CLIB_OK;
}

clib_status clib_slist_push_back(clib_slist *l, void *value)
{
    clib_slist_node *n;

    if (!l) return CLIB_ERR_INVAL;

    n = clib_alloc(l->alloc, sizeof *n);
    if (!n) return CLIB_ERR_NOMEM;

    n->value = value;
    n->next = NULL;
    if (l->tail) l->tail->next = n;
    else         l->head = n;
    l->tail = n;
    l->size++;
    return CLIB_OK;
}

void *clib_slist_pop_front(clib_slist *l)
{
    clib_slist_node *n;
    void *v;

    if (!l || !l->head) return NULL;

    n = l->head;
    v = n->value;
    l->head = n->next;
    if (!l->head) l->tail = NULL;
    clib_free(l->alloc, n);
    l->size--;
    return v;
}

void *clib_slist_front(const clib_slist *l)
{
    return (l && l->head) ? l->head->value : NULL;
}

clib_slist_node *clib_slist_find(const clib_slist *l, const void *value)
{
    clib_slist_node *n;
    if (!l) return NULL;
    for (n = l->head; n; n = n->next)
        if (n->value == value) return n;
    return NULL;
}

clib_status clib_slist_remove(clib_slist *l, const void *value)
{
    clib_slist_node *n, *prev = NULL;

    if (!l) return CLIB_ERR_INVAL;

    for (n = l->head; n; prev = n, n = n->next) {
        if (n->value != value) continue;

        if (prev) prev->next = n->next;
        else      l->head = n->next;
        if (l->tail == n) l->tail = prev;

        clib_free(l->alloc, n);
        l->size--;
        return CLIB_OK;
    }
    return CLIB_ERR_NOTFOUND;
}

void clib_slist_reverse(clib_slist *l)
{
    clib_slist_node *prev = NULL, *cur, *next;

    if (!l) return;
    l->tail = l->head;
    for (cur = l->head; cur; cur = next) {
        next = cur->next;
        cur->next = prev;
        prev = cur;
    }
    l->head = prev;
}

void clib_slist_foreach(const clib_slist *l, clib_slist_visit_fn fn, void *ctx)
{
    clib_slist_node *n;
    if (!l || !fn) return;
    for (n = l->head; n; n = n->next) fn(n->value, ctx);
}

void clib_slist_clear(clib_slist *l)
{
    clib_slist_node *n, *next;
    if (!l) return;
    for (n = l->head; n; n = next) {
        next = n->next;
        clib_free(l->alloc, n);
    }
    l->head = l->tail = NULL;
    l->size = 0;
}

void clib_slist_free(clib_slist *l)
{
    clib_slist_clear(l);
}
