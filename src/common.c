#include <clib/common.h>
#include <stdlib.h>
#include <string.h>

const char *clib_status_string(clib_status s)
{
    switch (s) {
    case CLIB_OK:          return "ok";
    case CLIB_ERR_NOMEM:   return "out of memory";
    case CLIB_ERR_FULL:    return "container full";
    case CLIB_ERR_EMPTY:   return "container empty";
    case CLIB_ERR_NOTFOUND:return "not found";
    case CLIB_ERR_INVAL:   return "invalid argument";
    }
    return "unknown";
}

static void *default_alloc(void *ptr, size_t size, void *ctx)
{
    (void)ctx;
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

clib_allocator clib_default_allocator = { default_alloc, NULL };

void *clib_alloc(const clib_allocator *a, size_t size)
{
    clib_allocator def = clib_default_allocator;
    return (a ? a : &def)->alloc_fn(NULL, size, a ? a->ctx : NULL);
}

void *clib_realloc(const clib_allocator *a, void *ptr, size_t size)
{
    clib_allocator def = clib_default_allocator;
    return (a ? a : &def)->alloc_fn(ptr, size, a ? a->ctx : NULL);
}

void clib_free(const clib_allocator *a, void *ptr)
{
    clib_allocator def = clib_default_allocator;
    (a ? a : &def)->alloc_fn(ptr, 0, a ? a->ctx : NULL);
}

int clib_cmp_int(const void *lhs, const void *rhs, void *ctx)
{
    int a, b;
    (void)ctx;
    if (!lhs || !rhs) return 0;
    a = *(const int *)lhs;
    b = *(const int *)rhs;
    return (a > b) - (a < b);
}

int clib_cmp_str(const void *lhs, const void *rhs, void *ctx)
{
    const char *a, *b;
    (void)ctx;
    if (!lhs || !rhs) return 0;
    a = *(const char *const *)lhs;
    b = *(const char *const *)rhs;
    if (!a || !b) return (a > b) - (a < b);
    return strcmp(a, b);
}
