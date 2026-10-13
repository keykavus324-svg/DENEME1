#include <clib/dlist.h>

void clib_dlist_init(clib_dlist *l, const clib_allocator *a)
{
    if (!l) return;
    l->head = l->tail = NULL;
    l->size = 0;
    l->alloc = a;
}

static clib_status link_after(clib_dlist *l, clib_dlist_node *pos, void *value)
{
    clib_dlist_node *n = clib_alloc(l->alloc, sizeof *n);
    if (!n) return CLIB_ERR_NOMEM;

    n->value = value;
    n->prev = pos;
    n->next = pos ? pos->next : l->head;

    if (n->next) n->next->prev = n;
    else         l->tail = n;
    if (pos)     pos->next = n;
    else         l->head = n;

    l->size++;
    return CLIB_OK;
}

clib_status clib_dlist_push_front(clib_dlist *l, void *value)
{
    if (!l) return CLIB_ERR_INVAL;
    if (!l->head) return link_after(l, NULL, value);

    {
        clib_dlist_node *n = clib_alloc(l->alloc, sizeof *n);
        if (!n) return CLIB_ERR_NOMEM;
        n->value = value;
        n->prev = NULL;
        n->next = l->head;
        l->head->prev = n;
        l->head = n;
        l->size++;
        return CLIB_OK;
    }
}

clib_status clib_dlist_push_back(clib_dlist *l, void *value)
{
    if (!l) return CLIB_ERR_INVAL;
    return link_after(l, l->tail, value);
}

void *clib_dlist_pop_front(clib_dlist *l)
{
    clib_dlist_node *n;
    void *v;

    if (!l || !l->head) return NULL;
    n = l->head;
    v = n->value;
    clib_dlist_erase(l, n);
    return v;
}

void *clib_dlist_pop_back(clib_dlist *l)
{
    clib_dlist_node *n;
    void *v;

    if (!l || !l->tail) return NULL;
    n = l->tail;
    v = n->value;
    clib_dlist_erase(l, n);
    return v;
}

void *clib_dlist_front(const clib_dlist *l)
{
    return (l && l->head) ? l->head->value : NULL;
}

void *clib_dlist_back(const clib_dlist *l)
{
    return (l && l->tail) ? l->tail->value : NULL;
}

clib_status clib_dlist_insert_before(clib_dlist *l, clib_dlist_node *pos, void *value)
{
    if (!l) return CLIB_ERR_INVAL;
    if (!pos) return clib_dlist_push_back(l, value);
    return link_after(l, pos->prev, value);
}

void clib_dlist_erase(clib_dlist *l, clib_dlist_node *n)
{
    if (!l || !n) return;

    if (n->prev) n->prev->next = n->next;
    else         l->head = n->next;
    if (n->next) n->next->prev = n->prev;
    else         l->tail = n->prev;

    clib_free(l->alloc, n);
    l->size--;
}

void clib_dlist_move_to_front(clib_dlist *l, clib_dlist_node *n)
{
    if (!l || !n || l->head == n) return;

    if (n->prev) n->prev->next = n->next;
    if (n->next) n->next->prev = n->prev;
    if (l->tail == n) l->tail = n->prev;

    n->prev = NULL;
    n->next = l->head;
    if (l->head) l->head->prev = n;
    l->head = n;
    if (!l->tail) l->tail = n;
}

void clib_dlist_foreach(const clib_dlist *l, clib_dlist_visit_fn fn, void *ctx)
{
    clib_dlist_node *n;
    if (!l || !fn) return;
    for (n = l->head; n; n = n->next) fn(n->value, ctx);
}

void clib_dlist_clear(clib_dlist *l)
{
    clib_dlist_node *n, *next;
    if (!l) return;
    for (n = l->head; n; n = next) {
        next = n->next;
        clib_free(l->alloc, n);
    }
    l->head = l->tail = NULL;
    l->size = 0;
}

void clib_dlist_free(clib_dlist *l)
{
    clib_dlist_clear(l);
}
