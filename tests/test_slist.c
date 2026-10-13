#include "clib_test.h"
#include <clib/slist.h>

static int collect_sum;
static void sum_visitor(void *value, void *ctx)
{
    *(int *)ctx += *(int *)value;
}

CLIB_TEST(test_slist)
{
    clib_slist l;
    int a = 1, b = 2, c = 3;

    clib_slist_init(&l, NULL);
    CLIB_CHECK_EQ(l.size, 0);
    CLIB_CHECK(clib_slist_front(&l) == NULL);
    CLIB_CHECK(clib_slist_pop_front(&l) == NULL);

    /* push_back must be O(1): the tail pointer has to be maintained. */
    CLIB_CHECK(clib_slist_push_back(&l, &a) == CLIB_OK);
    CLIB_CHECK(clib_slist_push_back(&l, &b) == CLIB_OK);
    CLIB_CHECK(clib_slist_push_front(&l, &c) == CLIB_OK);

    CLIB_CHECK_EQ(l.size, 3);
    CLIB_CHECK(clib_slist_front(&l) == &c);
    CLIB_CHECK(l.tail == clib_slist_find(&l, &b));
    CLIB_CHECK(clib_slist_find(&l, &a) != NULL);

    collect_sum = 0;
    clib_slist_foreach(&l, sum_visitor, &collect_sum);
    CLIB_CHECK_EQ(collect_sum, 6);

    CLIB_CHECK(clib_slist_pop_front(&l) == &c);
    CLIB_CHECK_EQ(l.size, 2);

    /* Removing the head must also fix the head pointer. */
    CLIB_CHECK(clib_slist_remove(&l, &c) == CLIB_ERR_NOTFOUND);
    CLIB_CHECK(clib_slist_remove(&l, &a) == CLIB_OK);
    CLIB_CHECK_EQ(l.size, 1);
    CLIB_CHECK(clib_slist_front(&l) == &b);

    /* Reversing a two-element list must swap head and tail. */
    clib_slist_push_back(&l, &a);
    clib_slist_reverse(&l);
    CLIB_CHECK(clib_slist_front(&l) == &a);
    CLIB_CHECK(l.tail == clib_slist_find(&l, &b));

    clib_slist_free(&l);
    CLIB_CHECK_EQ(l.size, 0);
    CLIB_CHECK(l.head == NULL && l.tail == NULL);

    /* Values are borrowed: freeing the list must not touch them. */
    CLIB_CHECK_EQ(a, 1);

    /* OOM path: the list is left untouched when the node cannot be allocated. */
    {
        clib_slist f;
        clib_slist_init(&f, &clib_test_failing_allocator);
        clib_test_fail_after(0);
        CLIB_CHECK(clib_slist_push_back(&f, &a) == CLIB_ERR_NOMEM);
        CLIB_CHECK_EQ(f.size, 0);
        clib_test_fail_never();
        clib_slist_free(&f);
    }
}
