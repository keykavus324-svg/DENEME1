#include "clib_test.h"
#include <clib/graph.h>
#include <clib/strutil.h>

/* --------------------------------------------------------------- strutil */

CLIB_TEST(test_strutil)
{
    clib_strbuf b;
    size_t i;

    CLIB_CHECK(clib_strbuf_init(&b, NULL) == CLIB_OK);
    CLIB_CHECK_STR(clib_strbuf_cstr(&b), "");
    CLIB_CHECK_EQ(clib_strbuf_len(&b), 0);

    CLIB_CHECK(clib_strbuf_append(&b, "hello") == CLIB_OK);
    CLIB_CHECK_STR(clib_strbuf_cstr(&b), "hello");
    CLIB_CHECK_EQ(clib_strbuf_len(&b), 5);

    CLIB_CHECK(clib_strbuf_append(&b, ", world") == CLIB_OK);
    CLIB_CHECK_STR(clib_strbuf_cstr(&b), "hello, world");

    /* append_n with a length shorter than the source. */
    CLIB_CHECK(clib_strbuf_append_n(&b, "!!!", 2) == CLIB_OK);
    CLIB_CHECK_STR(clib_strbuf_cstr(&b), "hello, world!!");

    /* Growth past several doublings must keep the terminator in place. */
    for (i = 0; i < 500; i++)
        CLIB_CHECK(clib_strbuf_append(&b, "x") == CLIB_OK);
    CLIB_CHECK_EQ(clib_strbuf_len(&b), 514);   /* 12 + 2 + 500 */
    CLIB_CHECK_EQ(strlen(clib_strbuf_cstr(&b)), 514);

    /* appendf must format into the tail, not overwrite the head. */
    {
        clib_strbuf f;
        CLIB_CHECK(clib_strbuf_init(&f, NULL) == CLIB_OK);
        CLIB_CHECK(clib_strbuf_append(&f, "n=") == CLIB_OK);
        CLIB_CHECK(clib_strbuf_appendf(&f, "%d/%s", 42, "ok") == CLIB_OK);
        CLIB_CHECK_STR(clib_strbuf_cstr(&f), "n=42/ok");
        CLIB_CHECK_EQ(clib_strbuf_len(&f), 7);
        clib_strbuf_free(&f);
    }

    /* clear() keeps capacity but empties the string. */
    clib_strbuf_clear(&b);
    CLIB_CHECK_STR(clib_strbuf_cstr(&b), "");
    CLIB_CHECK_EQ(clib_strbuf_len(&b), 0);
    CLIB_CHECK(b.capacity > 0);

    CLIB_CHECK(clib_strbuf_append(&b, "again") == CLIB_OK);
    CLIB_CHECK_STR(clib_strbuf_cstr(&b), "again");
    clib_strbuf_free(&b);

    /* --- read-only helpers ------------------------------------------------ */

    CLIB_CHECK(clib_str_starts_with("clib_vector", "clib_"));
    CLIB_CHECK(!clib_str_starts_with("clib_vector", "vector"));
    CLIB_CHECK(clib_str_ends_with("clib_vector", "_vector"));
    CLIB_CHECK(!clib_str_ends_with("clib_vector", "clib"));

    CLIB_CHECK(clib_str_ieq("Makefile", "makefile"));
    CLIB_CHECK(clib_str_ieq("ABC", "abc"));
    CLIB_CHECK(!clib_str_ieq("ABC", "abd"));

    {
        char s1[] = "   trimmed   ";
        CLIB_CHECK_STR(clib_str_trim(s1), "trimmed");

        char s2[] = "\t\n x \n\t";
        CLIB_CHECK_STR(clib_str_trim(s2), "x");

        char s3[] = "   ";
        CLIB_CHECK_STR(clib_str_trim(s3), "");

        char s4[] = "";
        CLIB_CHECK_STR(clib_str_trim(s4), "");

        char s5[] = "no-space";
        CLIB_CHECK_STR(clib_str_trim(s5), "no-space");
    }

    /* OOM path. */
    {
        clib_strbuf f;
        clib_test_fail_after(0);
        CLIB_CHECK(clib_strbuf_init(&f, &clib_test_failing_allocator) == CLIB_ERR_NOMEM);
        CLIB_CHECK_STR(clib_strbuf_cstr(&f), "");   /* must not be a NULL deref */
        clib_test_fail_never();
        clib_strbuf_free(&f);
    }
}

/* ----------------------------------------------------------------- graph */

static size_t dfs_seen[16];
static size_t dfs_count;
static void dfs_record(size_t vertex, void *ctx)
{
    (void)ctx;
    if (dfs_count < 16) dfs_seen[dfs_count++] = vertex;
}

/**
 * 0 -4-> 1 -1-> 3 -3-> 4
 *  \         /
 *   1-> 2 -5/
 *   (2 -2-> 1)
 *
 * Shortest paths from 0:  d[1]=3 (0->2->1), d[2]=1, d[3]=4, d[4]=7
 * Vertex 5 is isolated.
 */
static clib_status build_sample(clib_graph *g)
{
    clib_status s;
    s = clib_graph_init(g, 6, NULL);
    if (s != CLIB_OK) return s;
    if ((s = clib_graph_add_edge(g, 0, 1, 4)) != CLIB_OK) return s;
    if ((s = clib_graph_add_edge(g, 0, 2, 1)) != CLIB_OK) return s;
    if ((s = clib_graph_add_edge(g, 2, 1, 2)) != CLIB_OK) return s;
    if ((s = clib_graph_add_edge(g, 2, 3, 5)) != CLIB_OK) return s;
    if ((s = clib_graph_add_edge(g, 1, 3, 1)) != CLIB_OK) return s;
    if ((s = clib_graph_add_edge(g, 3, 4, 3)) != CLIB_OK) return s;
    return CLIB_OK;
}

CLIB_TEST(test_graph)
{
    clib_graph g;
    long long dist[6];
    size_t prev[6];

    CLIB_CHECK(build_sample(&g) == CLIB_OK);
    CLIB_CHECK_EQ(clib_graph_vertices(&g), 6);
    CLIB_CHECK_EQ(clib_graph_edges(&g), 6);

    /* Out-of-range vertices must be rejected, not silently accepted. */
    CLIB_CHECK(clib_graph_add_edge(&g, 99, 0, 1) == CLIB_ERR_INVAL);
    CLIB_CHECK(clib_graph_add_edge(&g, 0, 99, 1) == CLIB_ERR_INVAL);
    CLIB_CHECK_EQ(clib_graph_edges(&g), 6);

    /* --- BFS: unweighted hop counts ------------------------------------- */
    CLIB_CHECK(clib_graph_bfs(&g, 0, dist) == CLIB_OK);
    CLIB_CHECK_EQ(dist[0], 0);
    CLIB_CHECK_EQ(dist[1], 1);
    CLIB_CHECK_EQ(dist[2], 1);
    CLIB_CHECK_EQ(dist[3], 2);
    CLIB_CHECK_EQ(dist[4], 3);
    CLIB_CHECK_EQ(dist[5], CLIB_GRAPH_UNREACHABLE);   /* isolated vertex */

    /* --- DFS reaches exactly the vertices BFS can ----------------------- */
    dfs_count = 0;
    CLIB_CHECK(clib_graph_dfs(&g, 0, dfs_record, NULL) == CLIB_OK);
    CLIB_CHECK_EQ(dfs_count, 5);            /* 0..4, not the isolated 5 */
    CLIB_CHECK_EQ(dfs_seen[0], 0);          /* source is reported first */
    {
        int saw1 = 0, saw2 = 0, saw3 = 0, saw4 = 0, i;
        for (i = 0; i < (int)dfs_count; i++) {
            if (dfs_seen[i] == 1) saw1 = 1;
            if (dfs_seen[i] == 2) saw2 = 1;
            if (dfs_seen[i] == 3) saw3 = 1;
            if (dfs_seen[i] == 4) saw4 = 1;
        }
        CLIB_CHECK(saw1 && saw2 && saw3 && saw4);
    }

    /* A NULL visitor must be accepted and simply do nothing. */
    CLIB_CHECK(clib_graph_dfs(&g, 0, NULL, NULL) == CLIB_OK);

    /* --- Dijkstra --------------------------------------------------------- */
    CLIB_CHECK(clib_graph_dijkstra(&g, 0, dist, prev) == CLIB_OK);
    CLIB_CHECK_EQ(dist[0], 0);
    CLIB_CHECK_EQ(dist[1], 3);    /* 0->2->1 beats the direct 0->1 of weight 4 */
    CLIB_CHECK_EQ(dist[2], 1);
    CLIB_CHECK_EQ(dist[3], 4);    /* 0->2->1->3 beats 0->2->3 */
    CLIB_CHECK_EQ(dist[4], 7);
    CLIB_CHECK_EQ(dist[5], CLIB_GRAPH_UNREACHABLE);

    /* The predecessor chain for 4 must walk back to 0. */
    {
        size_t at = 4;
        int steps = 0;
        while (at != 0 && steps < 10) {
            at = prev[at];
            steps++;
        }
        CLIB_CHECK_EQ(at, 0);
        /* Shortest path is 0 -> 2 -> 1 -> 3 -> 4, so walking back takes 4 hops. */
        CLIB_CHECK_EQ(steps, 4);
    }

    /* --- undirected convenience ------------------------------------------ */
    {
        clib_graph u;
        long long ud[3];
        CLIB_CHECK(clib_graph_init(&u, 3, NULL) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge_undirected(&u, 0, 1, 7) == CLIB_OK);
        CLIB_CHECK_EQ(clib_graph_edges(&u), 2);
        CLIB_CHECK(clib_graph_add_edge_undirected(&u, 1, 2, 1) == CLIB_OK);
        CLIB_CHECK(clib_graph_dijkstra(&u, 0, ud, prev) == CLIB_OK);
        CLIB_CHECK_EQ(ud[1], 7);
        CLIB_CHECK_EQ(ud[2], 8);
        clib_graph_free(&u);
    }

    /* --- degenerate sizes -------------------------------------------------- */
    {
        clib_graph e;
        CLIB_CHECK(clib_graph_init(&e, 0, NULL) == CLIB_OK);
        CLIB_CHECK_EQ(clib_graph_vertices(&e), 0);
        CLIB_CHECK(clib_graph_bfs(&e, 0, dist) == CLIB_ERR_INVAL);
        clib_graph_free(&e);
    }
    {
        clib_graph s1;
        long long d1[1];
        size_t p1[1];
        CLIB_CHECK(clib_graph_init(&s1, 1, NULL) == CLIB_OK);
        CLIB_CHECK(clib_graph_bfs(&s1, 0, d1) == CLIB_OK);
        CLIB_CHECK_EQ(d1[0], 0);
        CLIB_CHECK(clib_graph_dijkstra(&s1, 0, d1, p1) == CLIB_OK);
        CLIB_CHECK_EQ(d1[0], 0);
        clib_graph_free(&s1);
    }

    clib_graph_free(&g);
    CLIB_CHECK_EQ(clib_graph_vertices(&g), 0);
    CLIB_CHECK_EQ(clib_graph_edges(&g), 0);

    /* OOM path. */
    {
        clib_graph f;
        clib_test_fail_after(0);
        CLIB_CHECK(clib_graph_init(&f, 4, &clib_test_failing_allocator) == CLIB_ERR_NOMEM);
        clib_test_fail_never();
        clib_graph_free(&f);
    }
}

/* ------------------------------------------------- graph algorithms */

CLIB_TEST(test_graph_algs)
{
    clib_graph g;
    size_t order[8];
    size_t n_out;
    long long bf[8];
    size_t bfp[8];

    /* --- topological sort on a DAG ------------------------------------- */
    /*
     *   0 -> 1 -> 3 -> 4 -> 5
     *    \-> 2 ->/
     */
    CLIB_CHECK(clib_graph_init(&g, 6, NULL) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 0, 1, 1) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 0, 2, 1) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 1, 3, 1) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 2, 3, 1) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 3, 4, 1) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 4, 5, 1) == CLIB_OK);

    CLIB_CHECK(clib_graph_topo_sort(&g, order, &n_out) == CLIB_OK);
    CLIB_CHECK_EQ(n_out, 6);

    /* A valid order is one where every arc points forward. */
    {
        int pos[6], i, ok = 1;
        for (i = 0; i < 6; i++) pos[(int)order[i]] = i;
        for (i = 0; i < 6; i++) {
            clib_graph_edge *e;
            for (e = g.adj[i]; e; e = e->next)
                if (!(pos[i] < pos[(int)e->to])) ok = 0;
        }
        CLIB_CHECK(ok);
        CLIB_CHECK_EQ(order[0], 0);
        CLIB_CHECK_EQ(order[5], 5);
    }
    clib_graph_free(&g);

    /* --- a cycle must be reported, not silently truncated -------------- */
    {
        clib_graph c;
        CLIB_CHECK(clib_graph_init(&c, 3, NULL) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge(&c, 0, 1, 1) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge(&c, 1, 2, 1) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge(&c, 2, 0, 1) == CLIB_OK);
        CLIB_CHECK(clib_graph_topo_sort(&c, order, &n_out) == CLIB_ERR_INVAL);
        CLIB_CHECK_EQ(n_out, 0);        /* nothing can start a cycle */
        clib_graph_free(&c);
    }

    /* --- MST by Kruskal -------------------------------------------------- */
    /*
     *   0 -1- 1          edges sorted: (0,1)=1 (2,3)=1 (1,2)=2
     *   |      |                       (3,4)=3 (1,3)=4 (0,2)=5
     *   5      2   4
     *   |      |  /
     *   2 -1- 3 -3- 4
     *
     * Kruskal takes (0,1), (2,3), (1,2), (3,4) = total 7, and stops at n-1.
     */
    {
        clib_graph_mst_edge mst[8];
        size_t mst_n = 0;
        long long total = 0;
        size_t i;

        CLIB_CHECK(clib_graph_init(&g, 5, NULL) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge_undirected(&g, 0, 1, 1) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge_undirected(&g, 0, 2, 5) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge_undirected(&g, 1, 2, 2) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge_undirected(&g, 1, 3, 4) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge_undirected(&g, 2, 3, 1) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge_undirected(&g, 3, 4, 3) == CLIB_OK);

        CLIB_CHECK(clib_graph_mst(&g, mst, &mst_n) == CLIB_OK);
        CLIB_CHECK_EQ(mst_n, 4);                    /* n - 1 for a connected graph */

        for (i = 0; i < mst_n; i++) {
            total += mst[i].weight;
            CLIB_CHECK(mst[i].u < mst[i].v);        /* normalised endpoints */
        }
        CLIB_CHECK_EQ(total, 7);

        /* Weights must be non-decreasing: that is what "Kruskal" means. */
        for (i = 1; i < mst_n; i++)
            CLIB_CHECK(mst[i - 1].weight <= mst[i].weight);

        clib_graph_free(&g);
    }

    /* A disconnected graph yields a forest, not an error. */
    {
        clib_graph_mst_edge mst[8];
        size_t mst_n = 0;
        CLIB_CHECK(clib_graph_init(&g, 4, NULL) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge_undirected(&g, 0, 1, 1) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge_undirected(&g, 2, 3, 2) == CLIB_OK);
        CLIB_CHECK(clib_graph_mst(&g, mst, &mst_n) == CLIB_OK);
        CLIB_CHECK_EQ(mst_n, 2);                    /* n - components = 4 - 2 */
        clib_graph_free(&g);
    }

    /* --- Bellman-Ford, including negative arcs --------------------------- */
    /*
     *   0 -4-> 1 -(-3)-> 2 -2-> 3
     *    \--5------------>^     ^
     *                     6-----/
     * d[1]=4, d[2]=1 (via 0->1->2), d[3]=3 (via 2)
     */
    CLIB_CHECK(clib_graph_init(&g, 4, NULL) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 0, 1, 4) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 0, 2, 5) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 1, 2, -3) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 2, 3, 2) == CLIB_OK);
    CLIB_CHECK(clib_graph_add_edge(&g, 1, 3, 6) == CLIB_OK);

    CLIB_CHECK(clib_graph_bellman_ford(&g, 0, bf, bfp) == CLIB_OK);
    CLIB_CHECK_EQ(bf[0], 0);
    CLIB_CHECK_EQ(bf[1], 4);
    CLIB_CHECK_EQ(bf[2], 1);        /* negative arc beats the direct 0->2 of 5 */
    CLIB_CHECK_EQ(bf[3], 3);
    clib_graph_free(&g);

    /* A negative cycle must be detected, not looped over forever. */
    {
        clib_graph nc;
        CLIB_CHECK(clib_graph_init(&nc, 3, NULL) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge(&nc, 0, 1, 1) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge(&nc, 1, 2, -2) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge(&nc, 2, 1, -2) == CLIB_OK);
        CLIB_CHECK(clib_graph_bellman_ford(&nc, 0, bf, bfp) == CLIB_ERR_INVAL);
        clib_graph_free(&nc);
    }

    /* Unreachable vertices must stay marked, not wrapped around. */
    {
        clib_graph u;
        CLIB_CHECK(clib_graph_init(&u, 3, NULL) == CLIB_OK);
        CLIB_CHECK(clib_graph_add_edge(&u, 0, 1, 2) == CLIB_OK);
        CLIB_CHECK(clib_graph_bellman_ford(&u, 0, bf, bfp) == CLIB_OK);
        CLIB_CHECK_EQ(bf[0], 0);
        CLIB_CHECK_EQ(bf[1], 2);
        CLIB_CHECK_EQ(bf[2], CLIB_GRAPH_UNREACHABLE);
        clib_graph_free(&u);
    }

    /* Bad arguments must be rejected outright. */
    CLIB_CHECK(clib_graph_topo_sort(NULL, order, &n_out) == CLIB_ERR_INVAL);
    CLIB_CHECK(clib_graph_bellman_ford(&g, 0, bf, bfp) == CLIB_ERR_INVAL);  /* freed */
}
