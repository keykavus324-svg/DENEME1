#include "clib_test.h"
#include <clib/stack.h>

CLIB_TEST(test_stack)
{
    clib_stack s;
    int a = 1, b = 2, c = 3;

    clib_stack_init(&s, NULL);
    CLIB_CHECK_EQ(clib_stack_size(&s), 0);
    CLIB_CHECK(clib_stack_pop(&s) == NULL);
    CLIB_CHECK(clib_stack_peek(&s) == NULL);

    CLIB_CHECK(clib_stack_push(&s, &a) == CLIB_OK);
    CLIB_CHECK(clib_stack_push(&s, &b) == CLIB_OK);
    CLIB_CHECK(clib_stack_peek(&s) == &b);

    CLIB_CHECK(clib_stack_push(&s, &c) == CLIB_OK);
    CLIB_CHECK_EQ(clib_stack_size(&s), 3);

    /* LIFO order. */
    CLIB_CHECK(clib_stack_pop(&s) == &c);
    CLIB_CHECK(clib_stack_pop(&s) == &b);
    CLIB_CHECK(clib_stack_peek(&s) == &a);
    CLIB_CHECK(clib_stack_pop(&s) == &a);
    CLIB_CHECK_EQ(clib_stack_size(&s), 0);

    /* A drained stack must still be reusable. */
    CLIB_CHECK(clib_stack_push(&s, &a) == CLIB_OK);
    CLIB_CHECK(clib_stack_pop(&s) == &a);

    clib_stack_free(&s);
    CLIB_CHECK_EQ(clib_stack_size(&s), 0);

    /* Bulk push past several regrows. */
    {
        clib_stack t;
        int i;
        int vals[500];
        clib_stack_init(&t, NULL);
        for (i = 0; i < 500; i++) {
            vals[i] = i;
            CLIB_CHECK(clib_stack_push(&t, &vals[i]) == CLIB_OK);
        }
        CLIB_CHECK_EQ(clib_stack_size(&t), 500);
        for (i = 499; i >= 0; i--)
            CLIB_CHECK(clib_stack_pop(&t) == &vals[i]);
        clib_stack_free(&t);
    }
}
