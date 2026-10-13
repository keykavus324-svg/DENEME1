#include <clib/vector.h>
#include <string.h>

/* Keep the typed macro-struct and the generic core layout-compatible. */
typedef struct {
    int *data; size_t size, capacity; const clib_allocator *alloc; size_t es;
} clib_vector_probe;

_Static_assert(sizeof(clib_vector_probe) == sizeof(clib_vector_raw),
               "clib_vector(T) and clib_vector_raw layouts diverged");
_Static_assert(_Alignof(clib_vector_probe) == _Alignof(clib_vector_raw),
               "clib_vector(T) and clib_vector_raw alignment diverged");

clib_status clib_vector_init_raw(clib_vector_raw *v, size_t elem_size,
                                 const clib_allocator *a)
{
    if (!v || elem_size == 0) return CLIB_ERR_INVAL;
    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
    v->elem_size = elem_size;
    v->alloc = a;
    return CLIB_OK;
}

clib_status clib_vector_reserve_raw(clib_vector_raw *v, size_t extra)
{
    size_t need, cap;
    void *p;

    if (!v) return CLIB_ERR_INVAL;
    if (extra > (size_t)-1 - v->size) return CLIB_ERR_NOMEM;

    need = v->size + extra;
    if (need <= v->capacity) return CLIB_OK;

    cap = v->capacity ? v->capacity : 8;
    while (cap < need) {
        if (cap > ((size_t)-1) / 2) { cap = need; break; }
        cap *= 2;
    }
    if (cap > ((size_t)-1) / v->elem_size) return CLIB_ERR_NOMEM;

    p = clib_realloc(v->alloc, v->data, cap * v->elem_size);
    if (!p) return CLIB_ERR_NOMEM;

    v->data = p;
    v->capacity = cap;
    return CLIB_OK;
}

void clib_vector_clear(clib_vector_raw *v)
{
    if (v) v->size = 0;
}

clib_status clib_vector_resize_raw(clib_vector_raw *v, size_t n)
{
    if (!v) return CLIB_ERR_INVAL;
    if (n > v->size) {
        clib_status s = clib_vector_reserve_raw(v, n - v->size);
        if (s != CLIB_OK) return s;
        memset((char *)v->data + v->size * v->elem_size, 0,
               (n - v->size) * v->elem_size);
    }
    v->size = n;
    return CLIB_OK;
}

void clib_vector_free_raw(clib_vector_raw *v)
{
    if (!v) return;
    clib_free(v->alloc, v->data);
    v->data = NULL;
    v->size = v->capacity = 0;
}
