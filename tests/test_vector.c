#include "clib_test.h"
#include <clib/vector.h>

CLIB_TEST(test_vector)
{
    clib_vector(int) v;

    CLIB_CHECK(clib_vector_init(&v) == CLIB_OK);
    CLIB_CHECK_EQ(clib_vector_size(&v), 0);
    CLIB_CHECK_EQ(clib_vector_capacity(&v), 0);

    /* Growth must stay amortised: 1000 pushes, no error, values intact. */
    {
        int i;
        for (i = 0; i < 1000; i++)
            CLIB_CHECK(clib_vector_push(&v, i * i) == CLIB_OK);

        CLIB_CHECK_EQ(clib_vector_size(&v), 1000);
        CLIB_CHECK(clib_vector_capacity(&v) >= 1000);
        for (i = 0; i < 1000; i++)
            CLIB_CHECK_EQ(clib_vector_get(&v, i), i * i);
    }

    /* LIFO pop returns the pushed values in reverse. */
    {
        int i;
        for (i = 999; i >= 0; i--)
            CLIB_CHECK_EQ(clib_vector_pop(&v), i * i);
        CLIB_CHECK_EQ(clib_vector_size(&v), 0);
    }

    /* resize() up zeroes the new tail, down truncates. */
    CLIB_CHECK(clib_vector_resize_raw((clib_vector_raw *)&v, 5) == CLIB_OK);
    CLIB_CHECK_EQ(clib_vector_size(&v), 5);
    CLIB_CHECK_EQ(v.data[4], 0);
    CLIB_CHECK(clib_vector_resize_raw((clib_vector_raw *)&v, 2) == CLIB_OK);
    CLIB_CHECK_EQ(clib_vector_size(&v), 2);

    /* clear() keeps capacity, so a refill must not allocate again. */
    {
        size_t cap = clib_vector_capacity(&v);
        clib_vector_clear((clib_vector_raw *)&v);
        CLIB_CHECK_EQ(clib_vector_size(&v), 0);
        CLIB_CHECK_EQ(clib_vector_capacity(&v), cap);
    }

    clib_vector_free(&v);
    CLIB_CHECK_EQ(clib_vector_size(&v), 0);

    /* A failing allocator must surface as CLIB_ERR_NOMEM, not a crash. */
    {
        clib_vector(int) f;
        clib_test_fail_after(0);
        CLIB_CHECK(clib_vector_init_with(&f, &clib_test_failing_allocator) == CLIB_OK);
        CLIB_CHECK(clib_vector_push(&f, 1) == CLIB_ERR_NOMEM);
        CLIB_CHECK_EQ(clib_vector_size(&f), 0);   /* unchanged on failure */
        clib_test_fail_never();
        clib_vector_free(&f);
    }
}
