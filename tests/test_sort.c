#include "clib_test.h"
#include <clib/sort.h>

#define N 2000

/* Deterministic LCG so a failure is always reproducible. */
static unsigned long long rng_state = 88172645463325252u;
static unsigned long long rng_next(void)
{
    rng_state = rng_state * 6364136223846793005u + 1442695040888963407u;
    return rng_state >> 33;
}

/** (key, original index) - lets us assert stability. */
typedef struct kv {
    int key;
    int idx;
} kv;

static int cmp_kv(const void *lhs, const void *rhs, void *ctx)
{
    const kv *a = lhs, *b = rhs;
    (void)ctx;
    return (a->key > b->key) - (a->key < b->key);
}

static void fill_random(int *a, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) a[i] = (int)(rng_next() % 1000);
}

CLIB_TEST(test_sort)
{
    static int a[N], b[N];
    size_t i;

    /* --- quicksort on every nasty input shape --------------------------- */

    fill_random(a, N);
    clib_sort_quick(a, N, sizeof a[0], clib_cmp_int, NULL);
    CLIB_CHECK(clib_is_sorted(a, N, sizeof a[0], clib_cmp_int, NULL));

    /* Already sorted: the median-of-three pivot must not let this go O(n^2). */
    clib_sort_quick(a, N, sizeof a[0], clib_cmp_int, NULL);
    CLIB_CHECK(clib_is_sorted(a, N, sizeof a[0], clib_cmp_int, NULL));

    /* Reverse sorted. */
    for (i = 0; i < N; i++) a[i] = (int)(N - i);
    clib_sort_quick(a, N, sizeof a[0], clib_cmp_int, NULL);
    CLIB_CHECK(clib_is_sorted(a, N, sizeof a[0], clib_cmp_int, NULL));
    CLIB_CHECK_EQ(a[0], 1);
    CLIB_CHECK_EQ(a[N - 1], (int)N);

    /* ALL EQUAL - the case a naive 2-way partition turns into O(n^2). */
    for (i = 0; i < N; i++) a[i] = 7;
    clib_sort_quick(a, N, sizeof a[0], clib_cmp_int, NULL);
    CLIB_CHECK(clib_is_sorted(a, N, sizeof a[0], clib_cmp_int, NULL));
    for (i = 0; i < N; i++) CLIB_CHECK_EQ(a[i], 7);

    /* Two distinct values, heavily repeated. */
    for (i = 0; i < N; i++) a[i] = (int)(i % 2);
    clib_sort_quick(a, N, sizeof a[0], clib_cmp_int, NULL);
    CLIB_CHECK(clib_is_sorted(a, N, sizeof a[0], clib_cmp_int, NULL));

    /* Every size from 0 to 70, including the insertion-sort cutoff boundary. */
    for (i = 0; i <= 70; i++) {
        size_t j, n = i;
        for (j = 0; j < n; j++) a[j] = (int)(rng_next() % 100);
        clib_sort_quick(a, n, sizeof a[0], clib_cmp_int, NULL);
        CLIB_CHECK(clib_is_sorted(a, n, sizeof a[0], clib_cmp_int, NULL));
    }

    /* --- insertion sort -------------------------------------------------- */

    fill_random(a, 64);
    clib_sort_insertion(a, 64, sizeof a[0], clib_cmp_int, NULL);
    CLIB_CHECK(clib_is_sorted(a, 64, sizeof a[0], clib_cmp_int, NULL));

    /* --- merge sort stability ------------------------------------------- */

    {
        static kv pairs[N];
        for (i = 0; i < N; i++) {
            pairs[i].key = (int)(rng_next() % 50);   /* only 50 distinct keys */
            pairs[i].idx = (int)i;
        }
        CLIB_CHECK(clib_sort_merge(pairs, N, sizeof pairs[0], cmp_kv, NULL) == CLIB_OK);

        for (i = 1; i < N; i++) {
            /* Sorted by key... */
            CLIB_CHECK(pairs[i - 1].key <= pairs[i].key);
            /* ...and equal keys kept their original order: that IS stability. */
            if (pairs[i - 1].key == pairs[i].key)
                CLIB_CHECK(pairs[i - 1].idx < pairs[i].idx);
        }
    }

    /* merge sort matches quicksort on the same input */
    fill_random(a, N);
    memcpy(b, a, sizeof a);
    clib_sort_quick(a, N, sizeof a[0], clib_cmp_int, NULL);
    CLIB_CHECK(clib_sort_merge(b, N, sizeof b[0], clib_cmp_int, NULL) == CLIB_OK);
    CLIB_CHECK(memcmp(a, b, sizeof a) == 0);

    /* n == 0 and n == 1 must be harmless no-ops. */
    clib_sort_quick(a, 0, sizeof a[0], clib_cmp_int, NULL);
    clib_sort_merge(a, 1, sizeof a[0], clib_cmp_int, NULL);

    /* --- binary search ---------------------------------------------------- */

    {
        int x;
        for (i = 0; i < N; i++) a[i] = (int)i * 2;
        for (i = 0; i < N; i++) {
            x = (int)i * 2;
            CLIB_CHECK(clib_binary_search(&x, a, N, sizeof a[0], clib_cmp_int, NULL) == &a[i]);
        }
        x = -5;
        CLIB_CHECK(clib_binary_search(&x, a, N, sizeof a[0], clib_cmp_int, NULL) == NULL);
        x = (int)N * 2 + 100;
        CLIB_CHECK(clib_binary_search(&x, a, N, sizeof a[0], clib_cmp_int, NULL) == NULL);
    }

    /* --- reverse ---------------------------------------------------------- */

    for (i = 0; i < 10; i++) a[i] = (int)i;
    clib_reverse(a, 10, sizeof a[0]);
    for (i = 0; i < 10; i++) CLIB_CHECK_EQ(a[i], (int)(9 - i));

    /* Odd and even lengths, plus the degenerate sizes. */
    for (i = 0; i < 7; i++) a[i] = (int)i;
    clib_reverse(a, 7, sizeof a[0]);
    CLIB_CHECK_EQ(a[0], 6);
    CLIB_CHECK_EQ(a[6], 0);
    CLIB_CHECK_EQ(a[3], 3);   /* the middle element stays put */

    /* --- is_sorted edge cases --------------------------------------------- */

    CLIB_CHECK(clib_is_sorted(a, 0, sizeof a[0], clib_cmp_int, NULL));
    CLIB_CHECK(clib_is_sorted(a, 1, sizeof a[0], clib_cmp_int, NULL));

    /* NULL / zero-size guards must return early instead of dereferencing. */
    clib_sort_quick(NULL, 10, sizeof a[0], clib_cmp_int, NULL);
    clib_reverse(a, 10, 0);
    CLIB_CHECK(clib_is_sorted(NULL, 10, sizeof a[0], clib_cmp_int, NULL));
}
