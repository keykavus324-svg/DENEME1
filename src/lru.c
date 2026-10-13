#include <clib/lru.h>

void clib_lru_init(clib_lru *c, size_t capacity, clib_lru_free_fn on_evict,
                   void *ctx, const clib_allocator *a)
{
    if (!c) return;
    clib_hashmap_init(&c->map, a);
    clib_dlist_init(&c->order, a);
    c->capacity = capacity;
    c->on_evict = on_evict;
    c->on_evict_ctx = ctx;
    c->alloc = a;
}

/**
 * Unlink @p node and release everything it owns.
 *
 * Order matters: the entry's key pointer aliases the map's key copy, so the
 * map entry has to be removed *after* the callback has seen the key and never
 * before we are done reading it.
 */
static void drop_node(clib_lru *c, clib_dlist_node *node)
{
    clib_lru_entry *e;
    const char *key;

    if (!node) return;

    e = node->value;
    key = e->key;

    if (c->on_evict) c->on_evict(key, e->value, c->on_evict_ctx);

    clib_free(c->alloc, e);                 /* the entry struct */
    clib_dlist_erase(&c->order, node);      /* the list node */
    clib_hashmap_remove(&c->map, key);      /* the key copy - last */
}

/** Evict from the tail until one more entry fits. */
static void make_room(clib_lru *c)
{
    if (c->capacity == 0) return;                   /* 0 means unbounded */
    while (clib_hashmap_size(&c->map) >= c->capacity && c->order.tail)
        drop_node(c, c->order.tail);
}

void *clib_lru_get(clib_lru *c, const char *key)
{
    clib_dlist_node *node;

    if (!c || !key) return NULL;
    node = clib_hashmap_get(&c->map, key);
    if (!node) return NULL;

    /* A hit is a use: that reordering is the whole point of LRU over FIFO. */
    clib_dlist_move_to_front(&c->order, node);
    return ((clib_lru_entry *)node->value)->value;
}

int clib_lru_has(const clib_lru *c, const char *key)
{
    if (!c || !key) return 0;
    return clib_hashmap_has(&c->map, key);
}

clib_status clib_lru_put(clib_lru *c, const char *key, void *value)
{
    clib_dlist_node *node;
    clib_lru_entry *e;

    if (!c || !key) return CLIB_ERR_INVAL;

    node = clib_hashmap_get(&c->map, key);
    if (node) {
        e = node->value;
        e->value = value;
        clib_dlist_move_to_front(&c->order, node);
        return CLIB_OK;
    }

    make_room(c);

    e = clib_alloc(c->alloc, sizeof *e);
    if (!e) return CLIB_ERR_NOMEM;
    e->key = NULL;
    e->value = value;

    if (clib_dlist_push_front(&c->order, e) != CLIB_OK) {
        clib_free(c->alloc, e);
        return CLIB_ERR_NOMEM;
    }

    if (clib_hashmap_put(&c->map, key, c->order.head) != CLIB_OK) {
        clib_dlist_erase(&c->order, c->order.head);
        clib_free(c->alloc, e);
        return CLIB_ERR_NOMEM;
    }

    /* Alias the map's key copy instead of allocating a second one. */
    e->key = clib_hashmap_key(&c->map, key);
    return CLIB_OK;
}

clib_status clib_lru_remove(clib_lru *c, const char *key)
{
    clib_dlist_node *node;

    if (!c || !key) return CLIB_ERR_INVAL;
    node = clib_hashmap_get(&c->map, key);
    if (!node) return CLIB_ERR_NOTFOUND;

    drop_node(c, node);
    return CLIB_OK;
}

size_t clib_lru_size(const clib_lru *c)
{
    return c ? clib_hashmap_size(&c->map) : 0;
}

size_t clib_lru_capacity(const clib_lru *c)
{
    return c ? c->capacity : 0;
}

void clib_lru_clear(clib_lru *c)
{
    if (!c) return;

    /* Evict every entry, running the callback for each value. */
    while (c->order.head) drop_node(c, c->order.head);

    /*
     * drop_node() removes entries from the hash map but leaves its slot array
     * allocated - clib_hashmap_remove() only unlinks, it does not release the
     * table. Without this line every cache would leak one table on free.
     * (Found by LeakSanitizer.)
     */
    clib_hashmap_clear(&c->map);
}

void clib_lru_free(clib_lru *c)
{
    if (!c) return;
    clib_lru_clear(c);
}
