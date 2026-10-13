#include <clib/queue.h>

#define CLIB_QUEUE_MIN_CAP 8u

void clib_queue_init(clib_queue *q, const clib_allocator *a)
{
    if (!q) return;
    q->data = NULL;
    q->head = q->size = 0;
    q->capacity = 0;
    q->alloc = a;
}

/**
 * Double the ring and linearise it so that `head` is 0 again.
 * Doing the copy with two memcpys keeps the elements in FIFO order.
 */
static clib_status regrow(clib_queue *q)
{
    size_t cap = q->capacity ? q->capacity * 2 : CLIB_QUEUE_MIN_CAP;
    void **p;

    if (cap > ((size_t)-1) / sizeof *p) return CLIB_ERR_NOMEM;
    p = clib_alloc(q->alloc, cap * sizeof *p);
    if (!p) return CLIB_ERR_NOMEM;

    if (q->size) {
        size_t mask = q->capacity - 1;
        size_t first = q->capacity - q->head;      /* tail of the ring */
        if (first > q->size) first = q->size;
        {
            size_t i;
            for (i = 0; i < first; i++)
                p[i] = q->data[(q->head + i) & mask];
            for (; i < q->size; i++)
                p[i] = q->data[(q->head + i) & mask];
        }
    }

    clib_free(q->alloc, q->data);
    q->data = p;
    q->head = 0;
    q->capacity = cap;
    return CLIB_OK;
}

clib_status clib_queue_enqueue(clib_queue *q, void *value)
{
    if (!q) return CLIB_ERR_INVAL;
    if (q->size == q->capacity) {
        clib_status s = regrow(q);
        if (s != CLIB_OK) return s;
    }
    q->data[(q->head + q->size) & (q->capacity - 1)] = value;
    q->size++;
    return CLIB_OK;
}

void *clib_queue_dequeue(clib_queue *q)
{
    void *v;
    if (!q || q->size == 0) return NULL;

    v = q->data[q->head];
    q->head = (q->head + 1) & (q->capacity - 1);
    q->size--;
    if (q->size == 0) q->head = 0;
    return v;
}

void *clib_queue_peek(const clib_queue *q)
{
    if (!q || q->size == 0) return NULL;
    return q->data[q->head];
}

size_t clib_queue_size(const clib_queue *q)
{
    return q ? q->size : 0;
}

void clib_queue_free(clib_queue *q)
{
    if (!q) return;
    clib_free(q->alloc, q->data);
    q->data = NULL;
    q->head = q->size = q->capacity = 0;
}
