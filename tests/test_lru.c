#include "clib_test.h"
#include <clib/lru.h>

static int evicted_count;
static char evicted_keys[16][16];
static long long evicted_sum;

static void record_evict(const char *key, void *value, void *ctx)
{
    (void)ctx;
    if (evicted_count < 16) {
        snprintf(evicted_keys[evicted_count], sizeof evicted_keys[0], "%s", key);
        evicted_count++;
    }
    evicted_sum += *(int *)value;
}

CLIB_TEST(test_lru)
{
    clib_lru c;
    int a = 1, b = 2, d = 3, e = 4, f = 5;

    evicted_count = 0;
    evicted_sum = 0;

    clib_lru_init(&c, 3, record_evict, NULL, NULL);
    CLIB_CHECK_EQ(clib_lru_size(&c), 0);
    CLIB_CHECK_EQ(clib_lru_capacity(&c), 3);
    CLIB_CHECK(clib_lru_get(&c, "a") == NULL);
    CLIB_CHECK(!clib_lru_has(&c, "a"));

    /* Fill to capacity: [d, c, b] with b at the tail. */
    CLIB_CHECK(clib_lru_put(&c, "b", &b) == CLIB_OK);
    CLIB_CHECK(clib_lru_put(&c, "c", &d) == CLIB_OK);
    CLIB_CHECK(clib_lru_put(&c, "d", &e) == CLIB_OK);
    CLIB_CHECK_EQ(clib_lru_size(&c), 3);
    CLIB_CHECK_EQ(evicted_count, 0);

    CLIB_CHECK(clib_lru_get(&c, "b") == &b);
    CLIB_CHECK(clib_lru_get(&c, "c") == &d);
    CLIB_CHECK(clib_lru_get(&c, "d") == &e);

    /* Touching "b" makes it most recent, so "c" is now the victim. */
    CLIB_CHECK(clib_lru_get(&c, "b") == &b);
    CLIB_CHECK(clib_lru_put(&c, "e", &f) == CLIB_OK);
    CLIB_CHECK_EQ(evicted_count, 1);
    CLIB_CHECK_STR(evicted_keys[0], "c");
    CLIB_CHECK_EQ(clib_lru_size(&c), 3);
    CLIB_CHECK(clib_lru_get(&c, "c") == NULL);
    CLIB_CHECK(clib_lru_get(&c, "b") == &b);   /* survived because it was used */
    CLIB_CHECK(clib_lru_get(&c, "e") == &f);

    /* Re-putting an existing key replaces the value without growing the cache. */
    {
        int newer = 99;
        CLIB_CHECK(clib_lru_put(&c, "b", &newer) == CLIB_OK);
        CLIB_CHECK_EQ(clib_lru_size(&c), 3);
        CLIB_CHECK(clib_lru_get(&c, "b") == &newer);
        CLIB_CHECK_EQ(evicted_count, 1);
    }

    /* Eviction order must follow recency, not insertion order. */
    {
        clib_lru t;
        int v1 = 1, v2 = 2, v3 = 3, v4 = 4;
        evicted_count = 0;
        clib_lru_init(&t, 2, record_evict, NULL, NULL);
        CLIB_CHECK(clib_lru_put(&t, "k1", &v1) == CLIB_OK);
        CLIB_CHECK(clib_lru_put(&t, "k2", &v2) == CLIB_OK);
        CLIB_CHECK(clib_lru_get(&t, "k1") == &v1);      /* k1 becomes newest */
        CLIB_CHECK(clib_lru_put(&t, "k3", &v3) == CLIB_OK);
        CLIB_CHECK_STR(evicted_keys[0], "k2");          /* k2 was oldest */
        CLIB_CHECK(clib_lru_get(&t, "k2") == NULL);
        CLIB_CHECK(clib_lru_get(&t, "k1") == &v1);
        CLIB_CHECK(clib_lru_put(&t, "k4", &v4) == CLIB_OK);
        CLIB_CHECK_STR(evicted_keys[1], "k3");
        CLIB_CHECK_EQ(clib_lru_size(&t), 2);
        clib_lru_free(&t);
    }

    /* The entry key must alias the map's copy, not a stale buffer. */
    {
        char tmp[8];
        clib_lru s;
        int v = 7;
        clib_lru_init(&s, 4, NULL, NULL, NULL);
        memcpy(tmp, "zz", 3);
        CLIB_CHECK(clib_lru_put(&s, tmp, &v) == CLIB_OK);
        memset(tmp, 'X', sizeof tmp);
        CLIB_CHECK(clib_lru_get(&s, "zz") == &v);
        clib_lru_free(&s);
    }

    /* remove() must trigger the callback and free the slot. */
    {
        int before = evicted_count;
        clib_lru r;
        int v = 5;
        clib_lru_init(&r, 8, record_evict, NULL, NULL);
        CLIB_CHECK(clib_lru_put(&r, "gone", &v) == CLIB_OK);
        CLIB_CHECK(clib_lru_remove(&r, "gone") == CLIB_OK);
        CLIB_CHECK_EQ(evicted_count, before + 1);
        CLIB_CHECK_EQ(clib_lru_size(&r), 0);
        CLIB_CHECK(clib_lru_remove(&r, "gone") == CLIB_ERR_NOTFOUND);
        clib_lru_free(&r);
    }

    /* capacity 0 means unbounded. */
    {
        clib_lru u;
        int i;
        static int vals[100];
        clib_lru_init(&u, 0, NULL, NULL, NULL);
        for (i = 0; i < 100; i++) {
            vals[i] = i;
            CLIB_CHECK(clib_lru_put(&u, "same-key", &vals[i]) == CLIB_OK);
        }
        CLIB_CHECK_EQ(clib_lru_size(&u), 1);      /* one key, last value wins */
        CLIB_CHECK(clib_lru_get(&u, "same-key") == &vals[99]);
        clib_lru_free(&u);
    }

    /* clear() must invoke the callback for every remaining value. */
    {
        int before = evicted_count;
        clib_lru w;
        clib_lru_init(&w, 4, record_evict, NULL, NULL);
        CLIB_CHECK(clib_lru_put(&w, "x", &a) == CLIB_OK);
        CLIB_CHECK(clib_lru_put(&w, "y", &b) == CLIB_OK);
        clib_lru_clear(&w);
        CLIB_CHECK_EQ(evicted_count, before + 2);
        CLIB_CHECK_EQ(clib_lru_size(&w), 0);
        CLIB_CHECK(w.order.head == NULL && w.order.tail == NULL);
    }

    clib_lru_free(&c);
    CLIB_CHECK_EQ(clib_lru_size(&c), 0);

    /* OOM path: the cache must stay usable after a failed insertion. */
    {
        clib_lru o;
        int v = 1;
        clib_lru_init(&o, 4, NULL, NULL, &clib_test_failing_allocator);
        clib_test_fail_after(0);
        CLIB_CHECK(clib_lru_put(&o, "x", &v) == CLIB_ERR_NOMEM);
        CLIB_CHECK_EQ(clib_lru_size(&o), 0);
        CLIB_CHECK(o.order.head == NULL);
        clib_test_fail_never();
        clib_lru_free(&o);
    }
    (void)evicted_sum;
}
