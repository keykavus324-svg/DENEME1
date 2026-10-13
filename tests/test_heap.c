#include "clib_test.h"
#include <clib/heap.h>

#define HN 500

static unsigned long long rng_state = 123456789u;
static unsigned long long rng_next(void)
{
    rng_state = rng_state * 6364136223846793005u + 1442695040888963407u;
    return rng_state >> 33;
}

CLIB_TEST(test_heap)
{
    clib_heap h;
    static int vals[HN];
    static long long keys[HN];
    static long long out[HN];
    static long long sorted[HN];
    size_t a, b;
    int i;

    clib_heap_init(&h, NULL);
    CLIB_CHECK_EQ(clib_heap_size(&h), 0);
    CLIB_CHECK(clib_heap_peek(&h) == NULL);
    CLIB_CHECK(clib_heap_pop(&h) == NULL);

    /* Push in RANDOM order... */
    for (i = 0; i < HN; i++) {
        vals[i] = i;
        keys[i] = (long long)(rng_next() % 100000);
        CLIB_CHECK(clib_heap_push(&h, keys[i], &vals[i]) == CLIB_OK);
    }
    CLIB_CHECK_EQ(clib_heap_size(&h), HN);

    /* ...and pop them back out, recovering each priority by pointer index. */
    for (i = 0; i < HN; i++) {
        int *v = (int *)clib_heap_pop(&h);
        CLIB_CHECK(v != NULL);
        if (!v) return;
        out[i] = keys[(int)(v - vals)];
    }
    CLIB_CHECK_EQ(clib_heap_size(&h), 0);

    /* The popped sequence must be exactly the input keys, sorted ascending. */
    memcpy(sorted, keys, sizeof keys);
    for (a = 1; a < HN; a++) {          /* insertion sort: fine for 500 items */
        long long tmp = sorted[a];
        for (b = a; b > 0 && sorted[b - 1] > tmp; b--) sorted[b] = sorted[b - 1];
        sorted[b] = tmp;
    }
    for (i = 0; i < HN; i++) CLIB_CHECK_EQ(out[i], sorted[i]);

    /* peek() must not remove anything. */
    {
        int x = 1, y = 2;
        clib_heap_free(&h);
        clib_heap_init(&h, NULL);
        CLIB_CHECK(clib_heap_push(&h, 5, &x) == CLIB_OK);
        CLIB_CHECK(clib_heap_push(&h, 3, &y) == CLIB_OK);
        CLIB_CHECK(clib_heap_peek(&h) == &y);
        CLIB_CHECK_EQ(clib_heap_size(&h), 2);
        CLIB_CHECK(clib_heap_pop(&h) == &y);
        CLIB_CHECK(clib_heap_pop(&h) == &x);
        CLIB_CHECK_EQ(clib_heap_size(&h), 0);
    }

    /* Negative and duplicate priorities must both work. */
    {
        int p = 1, q = 2, r = 3;
        CLIB_CHECK(clib_heap_push(&h, -5, &p) == CLIB_OK);
        CLIB_CHECK(clib_heap_push(&h, 0, &q) == CLIB_OK);
        CLIB_CHECK(clib_heap_push(&h, -5, &r) == CLIB_OK);
        CLIB_CHECK(clib_heap_pop(&h) != NULL);   /* one of the two -5 entries */
        CLIB_CHECK(clib_heap_pop(&h) != NULL);   /* the other -5 entry */
        CLIB_CHECK(clib_heap_pop(&h) == &q);
        CLIB_CHECK_EQ(clib_heap_size(&h), 0);
    }
    clib_heap_free(&h);

    /* rebuild(): mutate every priority in place, then restore in O(n). */
    {
        clib_heap rh;
        static int items[64];
        size_t k;
        long long last;
        int count;

        clib_heap_init(&rh, NULL);
        for (k = 0; k < 64; k++) {
            items[k] = (int)k;
            CLIB_CHECK(clib_heap_push(&rh, (long long)k, &items[k]) == CLIB_OK);
        }
        /* Flip the ordering: the array is now a max-heap, i.e. invalid. */
        for (k = 0; k < rh.size; k++)
            rh.data[k].priority = 63 - rh.data[k].priority;
        clib_heap_rebuild(&rh);

        last = -1;
        count = 0;
        while (clib_heap_size(&rh)) {
            int *p = (int *)clib_heap_pop(&rh);
            size_t j;
            long long pr = -1;
            for (j = 0; j < 64; j++)
                if (&items[j] == p) { pr = 63 - (long long)j; break; }
            CLIB_CHECK(pr >= last);
            last = pr;
            count++;
        }
        CLIB_CHECK_EQ(count, 64);
        clib_heap_free(&rh);
    }

    /* OOM path: report failure, do not crash or silently drop elements. */
    {
        clib_heap f;
        int x = 1;
        clib_heap_init(&f, &clib_test_failing_allocator);
        clib_test_fail_after(0);
        CLIB_CHECK(clib_heap_push(&f, 1, &x) == CLIB_ERR_NOMEM);
        CLIB_CHECK_EQ(clib_heap_size(&f), 0);
        clib_test_fail_never();
        clib_heap_free(&f);
    }
}
