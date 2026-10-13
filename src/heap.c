#include <clib/heap.h>

#define CLIB_HEAP_MIN_CAP 8u

void clib_heap_init(clib_heap *h, const clib_allocator *a)
{
    if (!h) return;
    h->data = NULL;
    h->size = h->capacity = 0;
    h->alloc = a;
}

static clib_status grow(clib_heap *h)
{
    size_t cap = h->capacity ? h->capacity * 2 : CLIB_HEAP_MIN_CAP;
    clib_heap_item *p;

    if (cap > ((size_t)-1) / sizeof *p) return CLIB_ERR_NOMEM;
    p = clib_realloc(h->alloc, h->data, cap * sizeof *p);
    if (!p) return CLIB_ERR_NOMEM;

    h->data = p;
    h->capacity = cap;
    return CLIB_OK;
}

static void sift_up(clib_heap *h, size_t i)
{
    clib_heap_item tmp = h->data[i];

    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (h->data[parent].priority <= tmp.priority) break;
        h->data[i] = h->data[parent];
        i = parent;
    }
    h->data[i] = tmp;
}

/** Floyd's bottom-up sift-down: one assignment per level instead of two. */
static void sift_down(clib_heap *h, size_t i)
{
    clib_heap_item tmp = h->data[i];
    size_t half = h->size / 2;

    while (i < half) {
        size_t child = 2 * i + 1;
        if (child + 1 < h->size &&
            h->data[child + 1].priority < h->data[child].priority)
            child++;
        if (h->data[child].priority >= tmp.priority) break;
        h->data[i] = h->data[child];
        i = child;
    }
    h->data[i] = tmp;
}

clib_status clib_heap_push(clib_heap *h, long long priority, void *value)
{
    if (!h) return CLIB_ERR_INVAL;
    if (h->size == h->capacity) {
        clib_status s = grow(h);
        if (s != CLIB_OK) return s;
    }
    h->data[h->size].priority = priority;
    h->data[h->size].value = value;
    sift_up(h, h->size);
    h->size++;
    return CLIB_OK;
}

void *clib_heap_peek(const clib_heap *h)
{
    if (!h || h->size == 0) return NULL;
    return h->data[0].value;
}

void *clib_heap_pop(clib_heap *h)
{
    void *v;
    if (!h || h->size == 0) return NULL;

    v = h->data[0].value;
    h->size--;
    if (h->size) {
        h->data[0] = h->data[h->size];
        sift_down(h, 0);
    }
    return v;
}

size_t clib_heap_size(const clib_heap *h)
{
    return h ? h->size : 0;
}

void clib_heap_rebuild(clib_heap *h)
{
    size_t i;
    if (!h || h->size < 2) return;
    for (i = h->size / 2; i-- > 0; )
        sift_down(h, i);
}

void clib_heap_free(clib_heap *h)
{
    if (!h) return;
    clib_free(h->alloc, h->data);
    h->data = NULL;
    h->size = h->capacity = 0;
}
