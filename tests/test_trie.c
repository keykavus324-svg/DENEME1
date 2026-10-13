#include "clib_test.h"
#include <clib/trie.h>

#define SEEN_MAX 32
static char seen[SEEN_MAX][320];   /* wide enough for the long-key test */
static int nseen;

static void collect_visitor(const char *key, void *value, void *ctx)
{
    (void)value; (void)ctx;
    if (nseen < SEEN_MAX) {
        snprintf(seen[nseen], sizeof seen[0], "%s", key);
        nseen++;
    }
}

CLIB_TEST(test_trie)
{
    clib_trie t;
    int a = 1, b = 2, c = 3;

    CLIB_CHECK(clib_trie_init(&t, NULL) == CLIB_OK);
    CLIB_CHECK_EQ(clib_trie_size(&t), 0);
    CLIB_CHECK(clib_trie_get(&t, "x") == NULL);
    CLIB_CHECK(!clib_trie_has(&t, "x"));
    CLIB_CHECK_EQ(t.nodes, 1);            /* root only */

    CLIB_CHECK(clib_trie_put(&t, "cat", &a) == CLIB_OK);
    CLIB_CHECK(clib_trie_put(&t, "car", &b) == CLIB_OK);
    CLIB_CHECK(clib_trie_put(&t, "cart", &c) == CLIB_OK);
    CLIB_CHECK_EQ(clib_trie_size(&t), 3);

    CLIB_CHECK(clib_trie_get(&t, "cat") == &a);
    CLIB_CHECK(clib_trie_get(&t, "car") == &b);
    CLIB_CHECK(clib_trie_get(&t, "cart") == &c);

    /* Prefixes must NOT be reported as keys unless they were inserted. */
    CLIB_CHECK(clib_trie_get(&t, "ca") == NULL);
    CLIB_CHECK(!clib_trie_has(&t, "ca"));
    CLIB_CHECK(clib_trie_get(&t, "cats") == NULL);
    CLIB_CHECK(clib_trie_get(&t, "c") == NULL);

    /*
     * Shared prefixes share nodes: root -> c -> a is common to all three.
     *   "cat"  adds c, a, t               = 3 new
     *   "car"  reuses c and a, adds r     = 1 new
     *   "cart" reuses c, a and r, adds t  = 1 new
     * So 1 (root) + 3 + 1 + 1 = 6 nodes hold 3 keys.
     */
    CLIB_CHECK_EQ(t.nodes, 1 + 3 + 1 + 1);

    /* Overwrite keeps the key count. */
    CLIB_CHECK(clib_trie_put(&t, "cat", &c) == CLIB_OK);
    CLIB_CHECK_EQ(clib_trie_size(&t), 3);
    CLIB_CHECK(clib_trie_get(&t, "cat") == &c);

    /* Enumeration must come out lexicographic: car, cart, cat. */
    nseen = 0;
    CLIB_CHECK_EQ(clib_trie_collect(&t, "", collect_visitor, NULL), 3);
    CLIB_CHECK_EQ(nseen, 3);
    CLIB_CHECK_STR(seen[0], "car");
    CLIB_CHECK_STR(seen[1], "cart");
    CLIB_CHECK_STR(seen[2], "cat");

    /* Prefix filtering. */
    nseen = 0;
    CLIB_CHECK_EQ(clib_trie_collect(&t, "car", collect_visitor, NULL), 2);
    CLIB_CHECK_STR(seen[0], "car");
    CLIB_CHECK_STR(seen[1], "cart");

    nseen = 0;
    CLIB_CHECK_EQ(clib_trie_collect(&t, "cat", collect_visitor, NULL), 1);
    CLIB_CHECK_STR(seen[0], "cat");

    CLIB_CHECK_EQ(clib_trie_collect(&t, "z", collect_visitor, NULL), 0);
    CLIB_CHECK_EQ(clib_trie_collect(&t, "catx", collect_visitor, NULL), 0);

    /* remove() must prune the nodes that only existed for that key. */
    CLIB_CHECK(clib_trie_remove(&t, "nope") == NULL);
    CLIB_CHECK(clib_trie_remove(&t, "ca") == NULL);      /* prefix, not a key */

    {
        size_t before = t.nodes;
        CLIB_CHECK(clib_trie_remove(&t, "cart") == &c);
        CLIB_CHECK_EQ(clib_trie_size(&t), 2);
        CLIB_CHECK(!clib_trie_has(&t, "cart"));
        CLIB_CHECK_EQ(t.nodes, before - 1);              /* only the final 't' */
        CLIB_CHECK(clib_trie_get(&t, "car") == &b);      /* "car" survives */
    }

    /*
     * Removing "cat" prunes only its final 't'. The shared 'a' must survive,
     * because "car" still needs it - that is the whole point of pruning with a
     * `nkids || terminal` guard instead of blindly deleting the whole path.
     */
    {
        size_t before = t.nodes;
        CLIB_CHECK(clib_trie_remove(&t, "cat") == &c);
        CLIB_CHECK_EQ(clib_trie_size(&t), 1);
        CLIB_CHECK_EQ(t.nodes, before - 1);
    }
    CLIB_CHECK(clib_trie_get(&t, "car") == &b);
    CLIB_CHECK_EQ(t.nodes, 4);                           /* root, c, a, r */

    /* Repeated insert/delete must not leak nodes. */
    {
        size_t baseline = t.nodes;
        int i;
        for (i = 0; i < 200; i++) {
            CLIB_CHECK(clib_trie_put(&t, "temporary", &a) == CLIB_OK);
            CLIB_CHECK(clib_trie_remove(&t, "temporary") == &a);
        }
        CLIB_CHECK_EQ(t.nodes, baseline);
        CLIB_CHECK_EQ(clib_trie_size(&t), 1);
    }

    /* Empty string is a valid key (it terminates at the root). */
    {
        clib_trie e;
        int v = 9;
        clib_trie_init(&e, NULL);
        CLIB_CHECK(clib_trie_put(&e, "", &v) == CLIB_OK);
        CLIB_CHECK(clib_trie_get(&e, "") == &v);
        CLIB_CHECK_EQ(clib_trie_size(&e), 1);
        nseen = 0;
        CLIB_CHECK_EQ(clib_trie_collect(&e, "", collect_visitor, NULL), 1);
        CLIB_CHECK_STR(seen[0], "");
        CLIB_CHECK(clib_trie_remove(&e, "") == &v);
        CLIB_CHECK_EQ(clib_trie_size(&e), 0);
        clib_trie_free(&e);
    }

    /* Long keys must not overflow the enumeration buffer. */
    {
        clib_trie lt;
        char big[300];
        size_t i;
        clib_trie_init(&lt, NULL);
        for (i = 0; i < 299; i++) big[i] = 'a';
        big[299] = '\0';
        CLIB_CHECK(clib_trie_put(&lt, big, &a) == CLIB_OK);
        CLIB_CHECK(clib_trie_get(&lt, big) == &a);
        nseen = 0;
        CLIB_CHECK_EQ(clib_trie_collect(&lt, "", collect_visitor, NULL), 1);
        CLIB_CHECK_STR(seen[0], big);
        clib_trie_free(&lt);
    }

    /* clear() must drop every node but the root. */
    clib_trie_clear(&t);
    CLIB_CHECK_EQ(clib_trie_size(&t), 0);
    CLIB_CHECK_EQ(t.nodes, 1);
    CLIB_CHECK_EQ(t.root->nkids, 0);
    CLIB_CHECK(clib_trie_get(&t, "car") == NULL);

    /* Reusable after clear. */
    CLIB_CHECK(clib_trie_put(&t, "again", &a) == CLIB_OK);
    CLIB_CHECK(clib_trie_get(&t, "again") == &a);

    clib_trie_free(&t);
    CLIB_CHECK(t.root == NULL);
    CLIB_CHECK_EQ(clib_trie_size(&t), 0);
    clib_trie_free(&t);              /* double free must be a no-op */

    /* OOM path. */
    {
        clib_trie o;
        clib_test_fail_after(0);
        CLIB_CHECK(clib_trie_init(&o, &clib_test_failing_allocator) == CLIB_ERR_NOMEM);
        clib_test_fail_never();
        clib_trie_free(&o);
    }
}
