#include <clib/stack.h>

#define CLIB_STACK_MIN_CAP 8u

void clib_stack_init(clib_stack *s, const clib_allocator *a)
{
    if (!s) return;
    s->data = NULL;
    s->size = s->capacity = 0;
    s->alloc = a;
}

static clib_status grow(clib_stack *s)
{
    size_t cap = s->capacity ? s->capacity * 2 : CLIB_STACK_MIN_CAP;
    void **p;

    if (cap > ((size_t)-1) / sizeof *p) return CLIB_ERR_NOMEM;
    p = clib_realloc(s->alloc, s->data, cap * sizeof *p);
    if (!p) return CLIB_ERR_NOMEM;

    s->data = p;
    s->capacity = cap;
    return CLIB_OK;
}

clib_status clib_stack_push(clib_stack *s, void *value)
{
    if (!s) return CLIB_ERR_INVAL;
    if (s->size == s->capacity) {
        clib_status st = grow(s);
        if (st != CLIB_OK) return st;
    }
    s->data[s->size++] = value;
    return CLIB_OK;
}

void *clib_stack_pop(clib_stack *s)
{
    if (!s || s->size == 0) return NULL;
    return s->data[--s->size];
}

void *clib_stack_peek(const clib_stack *s)
{
    if (!s || s->size == 0) return NULL;
    return s->data[s->size - 1];
}

size_t clib_stack_size(const clib_stack *s)
{
    return s ? s->size : 0;
}

void clib_stack_free(clib_stack *s)
{
    if (!s) return;
    clib_free(s->alloc, s->data);
    s->data = NULL;
    s->size = s->capacity = 0;
}
