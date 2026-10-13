#include <clib/sort.h>
#include <stdlib.h>
#include <string.h>

/* Below this size the nine ninther samples cost more than they are worth. */
#define CLIB_SORT_NINTHER_MIN 40u

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

/**
 * Pointer to the median of three values, WITHOUT moving any element.
 *
 * The classic Bentley-McIlroy median-of-three physically sorts its three
 * samples into place. Sampling by value instead keeps the partition a pure
 * read of the range until it starts swapping, which is both cheaper and easier
 * to reason about.
 */
static const char *median3_of(const char *a, const char *b, const char *c,
                              clib_cmp_fn cmp, void *ctx)
{
    int ab = cmp(a, b, ctx);
    int bc = cmp(b, c, ctx);
    int ac = cmp(a, c, ctx);

    if (ab <= 0) {
        if (bc <= 0) return b;          /* a <= b <= c          */
        return (ac <= 0) ? c : a;       /* a <= b,  c < b       */
    } else {
        if (bc >= 0) return b;          /* c <= b <= a          */
        return (ac <= 0) ? a : c;       /* b < a,   b < c       */
    }
}

/**
 * Tukey's "ninther": the median of three medians-of-three, taken over nine
 * samples spread across the range.
 *
 * Why plain median-of-three is not enough here: the 3-way partition below
 * rotates its "greater than" region, so sorting an already-sorted array turns
 * it into a *rotated* array at the next level down. Median-of-three then
 * samples (second smallest, middle, smallest) and picks almost the minimum as
 * the pivot, giving a 1-vs-(n-1) split. Measured, that made already-sorted
 * input O(n^1.5) instead of O(n log n) - 34.1M comparisons versus 2.6M at
 * n = 160,000. Nine spread-out samples cannot be defeated by a single rotation.
 */
static const char *choose_pivot(const char *base, size_t n, size_t es,
                                clib_cmp_fn cmp, void *ctx)
{
    size_t step;

    if (n < CLIB_SORT_NINTHER_MIN)
        return median3_of(base, base + (n / 2) * es, base + (n - 1) * es,
                          cmp, ctx);

    step = n / 8;
    {
        const char *lo = median3_of(base + 0 * es,
                                    base + 1 * step * es,
                                    base + 2 * step * es, cmp, ctx);
        const char *mid = median3_of(base + 3 * step * es,
                                     base + 4 * step * es,
                                     base + 5 * step * es, cmp, ctx);
        const char *hi = median3_of(base + 6 * step * es,
                                    base + 7 * step * es,
                                    base + (n - 1) * es, cmp, ctx);
        return median3_of(lo, mid, hi, cmp, ctx);
    }
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

    memcpy(pivot, choose_pivot(base, n, es, cmp, ctx), es);

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

void *clib_binary_search(const void *key, void *base, size_t n,
                         size_t es, clib_cmp_fn cmp, void *ctx)
{
    size_t lo = 0, hi = n;

    if (!key || !base || !cmp || es == 0) return NULL;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        char *p = (char *)base + mid * es;
        int c = cmp(key, p, ctx);
        if (c == 0) return p;
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
