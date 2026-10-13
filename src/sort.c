#include <clib/sort.h>
#include <stdlib.h>
#include <string.h>

/* Byte-wise swap: correct for any element type, no aliasing assumptions. */
static void swap_bytes(char *a, char *b, size_t es)
{
    while (es--) {
        char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

static void insertion_range(char *base, size_t n, size_t es,
                            clib_cmp_fn cmp, void *ctx)
{
    size_t i, j;
    for (i = 1; i < n; i++)
        for (j = i; j > 0; j--) {
            if (cmp(base + (j - 1) * es, base + j * es, ctx) <= 0) break;
            swap_bytes(base + (j - 1) * es, base + j * es, es);
        }
}

void clib_sort_insertion(void *base, size_t n, size_t es,
                         clib_cmp_fn cmp, void *ctx)
{
    if (!base || !cmp || n < 2 || es == 0) return;
    insertion_range(base, n, es, cmp, ctx);
}

/** Sort a[0], b[0], c[0] so that b ends up holding the median. */
static void order3(char *a, char *b, char *c, size_t es,
                   clib_cmp_fn cmp, void *ctx)
{
    if (cmp(a, b, ctx) > 0) swap_bytes(a, b, es);
    if (cmp(b, c, ctx) > 0) swap_bytes(b, c, es);
    if (cmp(a, b, ctx) > 0) swap_bytes(a, b, es);
}

/**
 * Dutch-national-flag partition.
 * On return: [0,lt) < pivot, [lt,gt) == pivot, [gt,n) > pivot.
 */
static void partition3(char *base, size_t n, size_t es,
                       clib_cmp_fn cmp, void *ctx, size_t *lt, size_t *gt)
{
    char inline_buf[128];
    char *pivot = (es <= sizeof inline_buf) ? inline_buf : (char *)malloc(es);
    size_t lo = 0, mid = 0, hi = n;

    if (!pivot) { *lt = 0; *gt = n; return; }   /* degenerate: treat as one block */

    order3(base, base + (n / 2) * es, base + (n - 1) * es, es, cmp, ctx);
    memcpy(pivot, base + (n / 2) * es, es);

    while (mid < hi) {
        int c = cmp(base + mid * es, pivot, ctx);
        if (c < 0) {
            swap_bytes(base + lo * es, base + mid * es, es);
            lo++; mid++;
        } else if (c > 0) {
            hi--;
            swap_bytes(base + mid * es, base + hi * es, es);
        } else {
            mid++;
        }
    }

    if (pivot != inline_buf) free(pivot);
    *lt = lo;
    *gt = hi;
}

/* Only the smaller side recurses; the bigger side becomes the next loop pass. */
static void quick_loop(char *base, size_t n, size_t es,
                       clib_cmp_fn cmp, void *ctx)
{
    while (n > CLIB_SORT_INSERTION_CUTOFF) {
        size_t lt, gt;
        partition3(base, n, es, cmp, ctx, &lt, &gt);

        if (lt < n - gt) {
            quick_loop(base, lt, es, cmp, ctx);
            base += gt * es;
            n -= gt;
        } else {
            quick_loop(base + gt * es, n - gt, es, cmp, ctx);
            n = lt;
        }
    }
}

void clib_sort_quick(void *base, size_t n, size_t es,
                     clib_cmp_fn cmp, void *ctx)
{
    if (!base || !cmp || n < 2 || es == 0) return;
    quick_loop(base, n, es, cmp, ctx);
    insertion_range(base, n, es, cmp, ctx);   /* tidy up the small partitions */
}

clib_status clib_sort_merge(void *base, size_t n, size_t es,
                            clib_cmp_fn cmp, void *ctx)
{
    char *scratch;
    size_t width;
    char *src, *dst;

    if (!base || !cmp) return CLIB_ERR_INVAL;
    if (n < 2 || es == 0) return CLIB_OK;
    if (n > ((size_t)-1) / es) return CLIB_ERR_NOMEM;

    scratch = malloc(n * es);
    if (!scratch) return CLIB_ERR_NOMEM;

    src = base;
    dst = scratch;

    /* Bottom-up: merge runs of `width`, then double it. */
    for (width = 1; width < n; width *= 2) {
        size_t lo;
        for (lo = 0; lo < n; lo += 2 * width) {
            size_t mid = lo + width < n ? lo + width : n;
            size_t hi  = mid + width < n ? mid + width : n;
            size_t i = lo, j = mid, k = lo;

            while (i < mid && j < hi) {
                /* <= keeps the sort stable: equal elements keep their order. */
                int c = cmp(src + i * es, src + j * es, ctx);
                memcpy(dst + (k++) * es, src + (c <= 0 ? i++ : j++) * es, es);
            }
            while (i < mid) memcpy(dst + (k++) * es, src + (i++) * es, es);
            while (j < hi)  memcpy(dst + (k++) * es, src + (j++) * es, es);
        }
        { char *t = src; src = dst; dst = t; }
    }

    /* An odd number of passes leaves the result in the scratch buffer. */
    if (src != base) memcpy(base, src, n * es);

    free(scratch);
    return CLIB_OK;
}

void *clib_binary_search(const void *key, const void *base, size_t n,
                         size_t es, clib_cmp_fn cmp, void *ctx)
{
    size_t lo = 0, hi = n;

    if (!key || !base || !cmp || es == 0) return NULL;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const char *p = (const char *)base + mid * es;
        int c = cmp(key, p, ctx);
        if (c == 0) return (void *)p;
        if (c < 0) hi = mid;
        else       lo = mid + 1;
    }
    return NULL;
}

int clib_is_sorted(const void *base, size_t n, size_t es,
                   clib_cmp_fn cmp, void *ctx)
{
    size_t i;
    if (!base || !cmp || es == 0) return 1;
    for (i = 1; i < n; i++)
        if (cmp((const char *)base + (i - 1) * es,
                (const char *)base + i * es, ctx) > 0)
            return 0;
    return 1;
}

void clib_reverse(void *base, size_t n, size_t es)
{
    char *a, *b;
    if (!base || n < 2 || es == 0) return;
    a = base;
    b = (char *)base + (n - 1) * es;
    while (a < b) {
        swap_bytes(a, b, es);
        a += es;
        b -= es;
    }
}
