#include "clib_test.h"

CLIB_TEST(test_common)
{
    CLIB_CHECK(strcmp(clib_status_string(CLIB_OK), "ok") == 0);
    CLIB_CHECK(strcmp(clib_status_string(CLIB_ERR_NOMEM), "out of memory") == 0);
    CLIB_CHECK(strcmp(clib_status_string((clib_status)999), "unknown") == 0);

    {
        int a = 1, b = 2, c = 1;
        CLIB_CHECK(clib_cmp_int(&a, &b, NULL) < 0);
        CLIB_CHECK(clib_cmp_int(&b, &a, NULL) > 0);
        CLIB_CHECK(clib_cmp_int(&a, &c, NULL) == 0);
    }

    {
        const char *x = "alpha", *y = "beta", *z = "alpha";
        CLIB_CHECK(clib_cmp_str(&x, &y, NULL) < 0);
        CLIB_CHECK(clib_cmp_str(&y, &x, NULL) > 0);
        CLIB_CHECK(clib_cmp_str(&x, &z, NULL) == 0);
    }

    /* The default allocator must treat size == 0 as "free and return NULL". */
    {
        void *p = clib_alloc(NULL, 32);
        CLIB_CHECK(p != NULL);
        p = clib_realloc(NULL, p, 64);
        CLIB_CHECK(p != NULL);
        clib_free(NULL, p);
    }

    /* A custom allocator is actually honoured, not silently replaced. */
    {
        clib_test_counter_reset();
        void *p = clib_alloc(&clib_test_counting_allocator, 16);
        CLIB_CHECK(p != NULL);
        CLIB_CHECK_EQ(clib_test_counter_state.live, 1);
        clib_free(&clib_test_counting_allocator, p);
        CLIB_CHECK_EQ(clib_test_counter_state.live, 0);
    }
}
