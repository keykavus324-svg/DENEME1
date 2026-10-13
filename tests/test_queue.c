#include "clib_test.h"
#include <clib/queue.h>

CLIB_TEST(test_queue)
{
    clib_queue q;
    int vals[300];
    int i;

    for (i = 0; i < 300; i++) vals[i] = i;

    clib_queue_init(&q, NULL);
    CLIB_CHECK_EQ(clib_queue_size(&q), 0);
    CLIB_CHECK(clib_queue_dequeue(&q) == NULL);
    CLIB_CHECK(clib_queue_peek(&q) == NULL);

    /* FIFO across a wrap: enqueue/dequeue interleaved so the ring turns over. */
    for (i = 0; i < 100; i++)
        CLIB_CHECK(clib_queue_enqueue(&q, &vals[i]) == CLIB_OK);
    CLIB_CHECK_EQ(clib_queue_size(&q), 100);

    for (i = 0; i < 50; i++) {
        CLIB_CHECK(clib_queue_peek(&q) == &vals[i]);
        CLIB_CHECK(clib_queue_dequeue(&q) == &vals[i]);
    }
    CLIB_CHECK_EQ(clib_queue_size(&q), 50);

    /* Capacity is a power of two - that is what makes the masking valid. */
    CLIB_CHECK((q.capacity & (q.capacity - 1)) == 0);

    for (i = 100; i < 300; i++)
        CLIB_CHECK(clib_queue_enqueue(&q, &vals[i]) == CLIB_OK);

    for (i = 50; i < 300; i++)
        CLIB_CHECK(clib_queue_dequeue(&q) == &vals[i]);

    CLIB_CHECK_EQ(clib_queue_size(&q), 0);
    CLIB_CHECK_EQ(q.head, 0);   /* head must reset once drained */

    /* Reuse after draining. */
    CLIB_CHECK(clib_queue_enqueue(&q, &vals[0]) == CLIB_OK);
    CLIB_CHECK(clib_queue_dequeue(&q) == &vals[0]);

    clib_queue_free(&q);
    CLIB_CHECK_EQ(clib_queue_size(&q), 0);

    /* A failing allocator must not corrupt the ring. */
    {
        clib_queue f;
        clib_queue_init(&f, &clib_test_failing_allocator);
        clib_test_fail_after(0);
        CLIB_CHECK(clib_queue_enqueue(&f, &vals[0]) == CLIB_ERR_NOMEM);
        CLIB_CHECK_EQ(clib_queue_size(&f), 0);
        clib_test_fail_never();
        clib_queue_free(&f);
    }
}
