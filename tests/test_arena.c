#include "clib_test.h"
#include <clib/arena.h>
#include <clib/hashmap.h>
#include <clib/vector.h>

CLIB_TEST(test_arena)
{
    clib_arena a;

    clib_arena_init(&a, 0, NULL);
    CLIB_CHECK_EQ(clib_arena_used(&a), 0);
    CLIB_CHECK_EQ(clib_arena_total(&a), 0);

    /* Zero-size requests must return NULL rather than a bogus pointer. */
    CLIB_CHECK(clib_arena_alloc(&a, 0, 0) == NULL);

    /* Basic bumping: successive allocations are distinct and usable. */
    {
        char *p = clib_arena_alloc(&a, 10, 0);
        char *q = clib_arena_alloc(&a, 20, 0);
        CLIB_CHECK(p != NULL && q != NULL);
        CLIB_CHECK(p != q);
        memset(p, 'x', 10);
        memset(q, 'y', 20);
        CLIB_CHECK_EQ(clib_arena_used(&a), 30);
        CLIB_CHECK(clib_arena_total(&a) >= 30);      /* blocks are >= 1024 */
    }

    /* Alignment must actually be honoured. */
    {
        int i;
        for (i = 0; i < 8; i++) {
            void *p = clib_arena_alloc(&a, 1, 64);
            CLIB_CHECK(p != NULL);
            CLIB_CHECK_EQ(((size_t)p % 64u), 0);
        }
    }

    /* A request larger than the default block must still succeed. */
    {
        char *big = clib_arena_alloc(&a, 100000, 0);
        CLIB_CHECK(big != NULL);
        big[0] = 1;
        big[99999] = 2;                 /* writes must not overlap anything */
        CLIB_CHECK_EQ(big[99999], 2);
    }

    /* Non-power-of-two alignment is rejected rather than silently mishandled. */
    CLIB_CHECK(clib_arena_alloc(&a, 8, 3) == NULL);

    /* reset() keeps the blocks but hands the memory out again. */
    {
        size_t total_before = clib_arena_total(&a);
        clib_arena_reset(&a);
        CLIB_CHECK_EQ(clib_arena_used(&a), 0);
        CLIB_CHECK_EQ(clib_arena_total(&a), total_before);   /* no new block */
        {
            char *p = clib_arena_alloc(&a, 64, 0);
            CLIB_CHECK(p != NULL);
            CLIB_CHECK_EQ(clib_arena_used(&a), 64);
        }
    }

    clib_arena_free(&a);
    CLIB_CHECK_EQ(clib_arena_used(&a), 0);
    CLIB_CHECK_EQ(clib_arena_total(&a), 0);
    clib_arena_free(&a);                /* double free must be a no-op */

    /* --- the adapter: a real container living on an arena --------------- */
    {
        clib_arena ar;
        clib_allocator alloc;
        clib_hashmap m;
        static int vals[200];
        char key[32];
        int i;

        clib_arena_init(&ar, 0, NULL);
        clib_arena_allocator(&ar, &alloc);

        clib_hashmap_init(&m, &alloc);
        for (i = 0; i < 200; i++) {
            vals[i] = i;
            snprintf(key, sizeof key, "key-%d", i);
            CLIB_CHECK(clib_hashmap_put(&m, key, &vals[i]) == CLIB_OK);
        }
        CLIB_CHECK_EQ(clib_hashmap_size(&m), 200);
        for (i = 0; i < 200; i++) {
            snprintf(key, sizeof key, "key-%d", i);
            CLIB_CHECK(clib_hashmap_get(&m, key) == &vals[i]);
        }
        CLIB_CHECK(clib_arena_used(&ar) > 0);

        /* Growth via realloc has to work, since the map rehashes itself. */
        CLIB_CHECK(clib_hashmap_remove(&m, "key-0") == CLIB_OK);
        CLIB_CHECK_EQ(clib_hashmap_size(&m), 199);
        CLIB_CHECK(clib_hashmap_get(&m, "key-199") == &vals[199]);

        /* No per-object frees: one call drops the map AND the arena at once. */
        clib_arena_free(&ar);
        CLIB_CHECK_EQ(clib_arena_total(&ar), 0);
    }

    /* A vector on an arena exercises the in-place growth path. */
    {
        clib_arena ar;
        clib_allocator alloc;
        clib_vector(int) v;
        int i;

        clib_arena_init(&ar, 0, NULL);
        clib_arena_allocator(&ar, &alloc);

        CLIB_CHECK(clib_vector_init_with(&v, &alloc) == CLIB_OK);
        for (i = 0; i < 500; i++)
            CLIB_CHECK(clib_vector_push(&v, i) == CLIB_OK);
        CLIB_CHECK_EQ(clib_vector_size(&v), 500);
        for (i = 0; i < 500; i++)
            CLIB_CHECK_EQ(clib_vector_get(&v, i), i);

        clib_vector_free(&v);
        clib_arena_free(&ar);
    }

    /* OOM path: a failing backing allocator must propagate. */
    {
        clib_arena f;
        clib_allocator alloc;
        clib_test_fail_after(0);
        clib_arena_init(&f, 0, &clib_test_failing_allocator);
        clib_arena_allocator(&f, &alloc);
        CLIB_CHECK(clib_arena_alloc(&f, 16, 0) == NULL);
        CLIB_CHECK_EQ(clib_arena_used(&f), 0);
        clib_test_fail_never();
        clib_arena_free(&f);
    }
}
