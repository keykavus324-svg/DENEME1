#include "clib_test.h"
#include <clib/bst.h>
#include <clib/hashmap.h>

/* ---------------------------------------------------------------- hashmap */

static int hm_count;
static void hm_count_visitor(const char *key, void *value, void *ctx)
{
    (void)key; (void)value; (void)ctx;
    hm_count++;
}

CLIB_TEST(test_hashmap)
{
    clib_hashmap m;
    int a = 10, b = 20, c = 30, d = 40;

    clib_hashmap_init(&m, NULL);
    CLIB_CHECK_EQ(clib_hashmap_size(&m), 0);
    CLIB_CHECK(clib_hashmap_get(&m, "a") == NULL);
    CLIB_CHECK(clib_hashmap_remove(&m, "a") == CLIB_ERR_NOTFOUND);

    CLIB_CHECK(clib_hashmap_put(&m, "a", &a) == CLIB_OK);
    CLIB_CHECK(clib_hashmap_put(&m, "b", &b) == CLIB_OK);
    CLIB_CHECK(clib_hashmap_put(&m, "c", &c) == CLIB_OK);
    CLIB_CHECK_EQ(clib_hashmap_size(&m), 3);

    CLIB_CHECK(clib_hashmap_get(&m, "a") == &a);
    CLIB_CHECK(clib_hashmap_has(&m, "c"));
    CLIB_CHECK(!clib_hashmap_has(&m, "zzz"));

    /* Overwriting must not change the entry count. */
    CLIB_CHECK(clib_hashmap_put(&m, "a", &d) == CLIB_OK);
    CLIB_CHECK_EQ(clib_hashmap_size(&m), 3);
    CLIB_CHECK(clib_hashmap_get(&m, "a") == &d);

    /* Keys are COPIED: killing the source buffer must not matter. */
    {
        char tmp[8];
        memcpy(tmp, "tmp", 4);
        CLIB_CHECK(clib_hashmap_put(&m, tmp, &a) == CLIB_OK);
        memset(tmp, 'X', sizeof tmp);
        CLIB_CHECK(clib_hashmap_get(&m, "tmp") == &a);
    }

    /* Capacity must stay a power of two - the probe masking depends on it. */
    CLIB_CHECK((m.capacity & (m.capacity - 1)) == 0);

    /* 600 keys force several rehashes; every one must still be findable. */
    {
        static int vals[600];
        char key[32];
        int i;
        for (i = 0; i < 600; i++) {
            vals[i] = i;
            snprintf(key, sizeof key, "k%d", i);
            CLIB_CHECK(clib_hashmap_put(&m, key, &vals[i]) == CLIB_OK);
        }
        for (i = 0; i < 600; i++) {
            snprintf(key, sizeof key, "k%d", i);
            CLIB_CHECK(clib_hashmap_get(&m, key) == &vals[i]);
        }
    }

    /* Delete-heavy churn: the tombstone rebuild must keep things bounded. */
    {
        char key[32];
        int i;
        for (i = 0; i < 400; i++) {
            snprintf(key, sizeof key, "k%d", i);
            CLIB_CHECK(clib_hashmap_remove(&m, key) == CLIB_OK);
        }
        CLIB_CHECK(m.tombstones <= m.size);
        for (i = 400; i < 600; i++) {
            snprintf(key, sizeof key, "k%d", i);
            CLIB_CHECK(clib_hashmap_get(&m, key) != NULL);
        }
        for (i = 0; i < 400; i++) {
            snprintf(key, sizeof key, "k%d", i);
            CLIB_CHECK(clib_hashmap_get(&m, key) == NULL);
        }
    }

    hm_count = 0;
    clib_hashmap_foreach(&m, hm_count_visitor, NULL);
    CLIB_CHECK_EQ(hm_count, (int)clib_hashmap_size(&m));

    clib_hashmap_free(&m);
    CLIB_CHECK_EQ(clib_hashmap_size(&m), 0);

    /* clib_hashmap_key() must hand back the map's own, durable copy. */
    {
        clib_hashmap k;
        char tmp[8];
        int x = 1;
        const char *stored;
        clib_hashmap_init(&k, NULL);
        memcpy(tmp, "abc", 4);
        CLIB_CHECK(clib_hashmap_put(&k, tmp, &x) == CLIB_OK);
        memset(tmp, 'X', sizeof tmp);

        stored = clib_hashmap_key(&k, "abc");
        CLIB_CHECK(stored != NULL);
        CLIB_CHECK_STR(stored, "abc");           /* not the scribbled buffer */
        CLIB_CHECK(stored != (const char *)tmp); /* it is the map's copy */
        CLIB_CHECK(clib_hashmap_key(&k, "nope") == NULL);
        clib_hashmap_free(&k);
    }

    /* Hash sanity: stable, order-sensitive, empty string == FNV offset basis. */
    CLIB_CHECK_EQ(clib_hash_str("abc"), clib_hash_str("abc"));
    CLIB_CHECK(clib_hash_str("abc") != clib_hash_str("bac"));
    CLIB_CHECK_EQ(clib_hash_str(""), (size_t)0xcbf29ce484222325u);

    /* OOM path. */
    {
        clib_hashmap f;
        clib_hashmap_init(&f, &clib_test_failing_allocator);
        clib_test_fail_after(0);
        CLIB_CHECK(clib_hashmap_put(&f, "x", &a) == CLIB_ERR_NOMEM);
        CLIB_CHECK_EQ(clib_hashmap_size(&f), 0);
        clib_test_fail_never();
        clib_hashmap_free(&f);
    }
}

/* -------------------------------------------------------------------- bst */

#define BST_SEEN_MAX 16
static char bst_seen[BST_SEEN_MAX][16];
static int bst_nseen;

static void bst_collect(const char *key, void *value, void *ctx)
{
    (void)value; (void)ctx;
    if (bst_nseen < BST_SEEN_MAX) {
        snprintf(bst_seen[bst_nseen], sizeof bst_seen[0], "%s", key);
        bst_nseen++;
    }
}

CLIB_TEST(test_bst)
{
    clib_bst t;
    static const char *keys[] = { "delta", "alpha", "zulu", "mike", "bravo", "echo" };
    static int vals[6] = { 4, 1, 26, 13, 2, 5 };
    size_t i;

    clib_bst_init(&t, NULL);
    CLIB_CHECK_EQ(clib_bst_size(&t), 0);
    CLIB_CHECK_EQ(clib_bst_height(&t), 0);
    CLIB_CHECK(clib_bst_get(&t, "alpha") == NULL);
    CLIB_CHECK(clib_bst_remove(&t, "alpha") == CLIB_ERR_NOTFOUND);

    for (i = 0; i < 6; i++)
        CLIB_CHECK(clib_bst_put(&t, keys[i], &vals[i]) == CLIB_OK);

    CLIB_CHECK_EQ(clib_bst_size(&t), 6);
    for (i = 0; i < 6; i++)
        CLIB_CHECK(clib_bst_get(&t, keys[i]) == &vals[i]);

    /* Overwrite keeps the size. */
    CLIB_CHECK(clib_bst_put(&t, "alpha", &vals[5]) == CLIB_OK);
    CLIB_CHECK_EQ(clib_bst_size(&t), 6);
    CLIB_CHECK(clib_bst_get(&t, "alpha") == &vals[5]);

    /* The in-order walk must come out sorted ascending. */
    {
        static const char *expected[] = { "alpha", "bravo", "delta", "echo", "mike", "zulu" };
        bst_nseen = 0;
        clib_bst_foreach(&t, bst_collect, NULL);
        CLIB_CHECK_EQ(bst_nseen, 6);
        for (i = 0; i < 6; i++)
            CLIB_CHECK_STR(bst_seen[i], expected[i]);
    }

    /* A NULL visitor must be a no-op, not a crash. */
    clib_bst_foreach(&t, NULL, NULL);

    /* Delete a leaf ("bravo") and a two-child node ("delta") - the hard case. */
    CLIB_CHECK(clib_bst_remove(&t, "bravo") == CLIB_OK);
    CLIB_CHECK_EQ(clib_bst_size(&t), 5);
    CLIB_CHECK(clib_bst_get(&t, "bravo") == NULL);

    CLIB_CHECK(clib_bst_remove(&t, "delta") == CLIB_OK);
    CLIB_CHECK_EQ(clib_bst_size(&t), 4);
    CLIB_CHECK(clib_bst_get(&t, "delta") == NULL);

    /* Re-linking must not have orphaned the rest of the tree. */
    CLIB_CHECK(clib_bst_get(&t, "alpha") == &vals[5]);
    CLIB_CHECK(clib_bst_get(&t, "zulu") == &vals[2]);
    CLIB_CHECK(clib_bst_get(&t, "mike") == &vals[3]);
    CLIB_CHECK(clib_bst_get(&t, "echo") == &vals[5]);
    CLIB_CHECK(clib_bst_height(&t) >= 1);

    /* Sorted inserts degenerate to a list - the documented limitation. */
    {
        static const char *sorted[] = { "alpha", "bravo", "delta", "echo" };
        clib_bst s;
        clib_bst_init(&s, NULL);
        for (i = 0; i < 4; i++)
            CLIB_CHECK(clib_bst_put(&s, sorted[i], &vals[i]) == CLIB_OK);
        /* 4 ascending keys -> a 4-deep chain: height == size. */
        CLIB_CHECK_EQ(clib_bst_height(&s), 4);
        clib_bst_free(&s);
    }

    /* The mirror image: descending inserts also degenerate. */
    {
        static const char *desc[] = { "zulu", "mike", "echo", "bravo" };
        clib_bst s;
        clib_bst_init(&s, NULL);
        for (i = 0; i < 4; i++)
            CLIB_CHECK(clib_bst_put(&s, desc[i], &vals[i]) == CLIB_OK);
        CLIB_CHECK_EQ(clib_bst_height(&s), 4);
        clib_bst_free(&s);
    }

    clib_bst_free(&t);
    CLIB_CHECK_EQ(clib_bst_size(&t), 0);
    CLIB_CHECK(t.root == NULL);

    /* OOM path. */
    {
        clib_bst f;
        clib_bst_init(&f, &clib_test_failing_allocator);
        clib_test_fail_after(0);
        CLIB_CHECK(clib_bst_put(&f, "x", &vals[0]) == CLIB_ERR_NOMEM);
        CLIB_CHECK_EQ(clib_bst_size(&f), 0);
        clib_test_fail_never();
        clib_bst_free(&f);
    }
}
