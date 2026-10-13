#include "clib_test.h"
#include <clib/dlist.h>

CLIB_TEST(test_dlist)
{
    clib_dlist l;
    int a = 1, b = 2, c = 3;

    clib_dlist_init(&l, NULL);
    CLIB_CHECK_EQ(l.size, 0);
    CLIB_CHECK(clib_dlist_pop_front(&l) == NULL);
    CLIB_CHECK(clib_dlist_pop_back(&l) == NULL);

    /* push_back a, push_back b, push_front c  ->  [c, a, b] */
    CLIB_CHECK(clib_dlist_push_back(&l, &a) == CLIB_OK);
    CLIB_CHECK(clib_dlist_push_back(&l, &b) == CLIB_OK);
    CLIB_CHECK(clib_dlist_push_front(&l, &c) == CLIB_OK);
    CLIB_CHECK_EQ(l.size, 3);
    CLIB_CHECK(clib_dlist_front(&l) == &c);
    CLIB_CHECK(clib_dlist_back(&l) == &b);

    /* Erasing the middle node is O(1) - this is what prev pointers buy. */
    {
        clib_dlist_node *mid = l.head->next;      /* the node holding &a */
        CLIB_CHECK(mid->value == &a);
        clib_dlist_erase(&l, mid);
        CLIB_CHECK_EQ(l.size, 2);                 /* [c, b] */
        CLIB_CHECK(clib_dlist_front(&l) == &c);
        CLIB_CHECK(clib_dlist_back(&l) == &b);
    }

    /* move_to_front is the LRU primitive: size must not change. */
    {
        clib_dlist_node *back = l.tail;           /* the node holding &b */
        CLIB_CHECK(back->value == &b);
        clib_dlist_move_to_front(&l, back);
        CLIB_CHECK_EQ(l.size, 2);                 /* [b, c] */
        CLIB_CHECK(clib_dlist_front(&l) == &b);
        CLIB_CHECK(clib_dlist_back(&l) == &c);
    }

    /* insert_before(NULL) behaves as push_back. */
    CLIB_CHECK(clib_dlist_insert_before(&l, NULL, &a) == CLIB_OK);   /* [b, c, a] */
    CLIB_CHECK(clib_dlist_back(&l) == &a);
    CLIB_CHECK_EQ(l.size, 3);

    CLIB_CHECK(clib_dlist_pop_front(&l) == &b);
    CLIB_CHECK(clib_dlist_pop_back(&l) == &a);
    CLIB_CHECK_EQ(l.size, 1);
    CLIB_CHECK(l.head == l.tail);

    clib_dlist_free(&l);
    CLIB_CHECK_EQ(l.size, 0);
    CLIB_CHECK(l.head == NULL && l.tail == NULL);

    /* A one-element list must keep head and tail consistent. */
    {
        clib_dlist s;
        clib_dlist_init(&s, NULL);
        CLIB_CHECK(clib_dlist_push_back(&s, &a) == CLIB_OK);
        CLIB_CHECK(s.head == s.tail);
        CLIB_CHECK(clib_dlist_pop_front(&s) == &a);
        CLIB_CHECK(s.head == NULL && s.tail == NULL);
        clib_dlist_free(&s);
    }

    /* erase() on the only node must clear both ends. */
    {
        clib_dlist s;
        clib_dlist_init(&s, NULL);
        CLIB_CHECK(clib_dlist_push_back(&s, &a) == CLIB_OK);
        clib_dlist_erase(&s, s.head);
        CLIB_CHECK_EQ(s.size, 0);
        CLIB_CHECK(s.head == NULL && s.tail == NULL);
        clib_dlist_free(&s);
    }

    /* OOM path. */
    {
        clib_dlist f;
        clib_dlist_init(&f, &clib_test_failing_allocator);
        clib_test_fail_after(0);
        CLIB_CHECK(clib_dlist_push_back(&f, &a) == CLIB_ERR_NOMEM);
        CLIB_CHECK_EQ(f.size, 0);
        clib_test_fail_never();
        clib_dlist_free(&f);
    }
}
