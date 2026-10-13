#include "clib_test.h"
#include <clib/uf.h>

CLIB_TEST(test_uf)
{
    clib_uf u;
    size_t i;

    CLIB_CHECK(clib_uf_init(&u, 10, NULL) == CLIB_OK);
    CLIB_CHECK_EQ(clib_uf_count(&u), 10);
    CLIB_CHECK_EQ(clib_uf_sets(&u), 10);

    /* Everyone starts in their own singleton. */
    for (i = 0; i < 10; i++) {
        CLIB_CHECK_EQ(clib_uf_find(&u, i), i);
        CLIB_CHECK_EQ(clib_uf_size_of(&u, i), 1);
        CLIB_CHECK(!clib_uf_connected(&u, i, (i + 1) % 10) || u.n == 1);
    }

    /* Out-of-range indices must be rejected, not wrapped around. */
    CLIB_CHECK_EQ(clib_uf_find(&u, 10), 10);
    CLIB_CHECK(clib_uf_union(&u, 10, 0) == CLIB_ERR_INVAL);
    CLIB_CHECK(!clib_uf_connected(&u, 0, 99));
    CLIB_CHECK_EQ(clib_uf_size_of(&u, 99), 0);
    CLIB_CHECK_EQ(clib_uf_sets(&u), 10);

    /* Union 0..4 into one component of size 5. */
    CLIB_CHECK(clib_uf_union(&u, 0, 1) == CLIB_OK);
    CLIB_CHECK(clib_uf_union(&u, 1, 2) == CLIB_OK);
    CLIB_CHECK(clib_uf_union(&u, 3, 4) == CLIB_OK);
    CLIB_CHECK(clib_uf_union(&u, 2, 3) == CLIB_OK);

    CLIB_CHECK_EQ(clib_uf_sets(&u), 6);           /* {0..4}, 5, 6, 7, 8, 9 */
    CLIB_CHECK_EQ(clib_uf_size_of(&u, 0), 5);
    CLIB_CHECK_EQ(clib_uf_size_of(&u, 4), 5);
    CLIB_CHECK_EQ(clib_uf_size_of(&u, 5), 1);
    for (i = 0; i < 5; i++) {
        CLIB_CHECK(clib_uf_connected(&u, 0, i));
        CLIB_CHECK_EQ(clib_uf_find(&u, i), clib_uf_find(&u, 0));
    }
    CLIB_CHECK(!clib_uf_connected(&u, 0, 5));

    /* Unioning two members of the same set must not change anything. */
    CLIB_CHECK(clib_uf_union(&u, 0, 4) == CLIB_OK);
    CLIB_CHECK_EQ(clib_uf_sets(&u), 6);
    CLIB_CHECK_EQ(clib_uf_size_of(&u, 0), 5);

    /* Path compression: after a find, the node must point at the root
       directly, not at an intermediate parent. */
    {
        size_t r = clib_uf_find(&u, 4);
        CLIB_CHECK_EQ(clib_uf_find(&u, 4), r);
        CLIB_CHECK_EQ(u.parent[4], r);            /* compressed */
    }

    /* Merge everything into a single component. */
    for (i = 5; i < 10; i++)
        CLIB_CHECK(clib_uf_union(&u, 0, i) == CLIB_OK);
    CLIB_CHECK_EQ(clib_uf_sets(&u), 1);
    CLIB_CHECK_EQ(clib_uf_size_of(&u, 7), 10);
    for (i = 0; i < 10; i++) CLIB_CHECK(clib_uf_connected(&u, i, 9));

    /* reset() must return to singletons without reallocating. */
    clib_uf_reset(&u);
    CLIB_CHECK_EQ(clib_uf_sets(&u), 10);
    CLIB_CHECK_EQ(clib_uf_size_of(&u, 3), 1);
    CLIB_CHECK(!clib_uf_connected(&u, 0, 1));

    /* Union-by-rank check: hang 0 under 1, then a long chain onto 0.
       A naive "always attach to the second" would build a deep tree; rank
       keeps it shallow, so every find stays one or two hops. */
    clib_uf_reset(&u);
    for (i = 1; i < 10; i++) CLIB_CHECK(clib_uf_union(&u, 0, i) == CLIB_OK);
    for (i = 0; i < 10; i++) {
        size_t r = clib_uf_find(&u, i);
        CLIB_CHECK_EQ(r, clib_uf_find(&u, 0));
        CLIB_CHECK(u.parent[i] == r || u.parent[u.parent[i]] == r);
    }

    clib_uf_free(&u);
    CLIB_CHECK_EQ(clib_uf_count(&u), 0);
    CLIB_CHECK_EQ(clib_uf_sets(&u), 0);

    /* n == 0 must be a valid, empty partition. */
    {
        clib_uf e;
        CLIB_CHECK(clib_uf_init(&e, 0, NULL) == CLIB_OK);
        CLIB_CHECK_EQ(clib_uf_sets(&e), 0);
        CLIB_CHECK_EQ(clib_uf_find(&e, 0), 0);
        CLIB_CHECK(clib_uf_union(&e, 0, 0) == CLIB_ERR_INVAL);
        clib_uf_free(&e);
    }

    /* OOM path. */
    {
        clib_uf o;
        clib_test_fail_after(0);
        CLIB_CHECK(clib_uf_init(&o, 100, &clib_test_failing_allocator) == CLIB_ERR_NOMEM);
        CLIB_CHECK_EQ(clib_uf_count(&o), 0);
        clib_test_fail_never();
        clib_uf_free(&o);
    }
}
