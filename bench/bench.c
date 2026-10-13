/**
 * @file bench.c
 * @brief Micro-benchmarks for the containers and sorting routines.
 *
 * These are not careful laboratory measurements - they are here so that a
 * change which accidentally makes something quadratic shows up immediately.
 *
 * Build with:  make bench
 */
#include <clib/common.h>
#include <clib/graph.h>
#include <clib/hashmap.h>
#include <clib/heap.h>
#include <clib/queue.h>
#include <clib/slist.h>
#include <clib/sort.h>
#include <clib/strutil.h>
#include <clib/vector.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define N_SORT   200000
#define N_HASH   100000
#define N_VEC   1000000
#define N_HEAP   100000

static double now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

/* Volatile sink: results that are never used get optimised away otherwise. */
static volatile long long sink;

static void report(const char *name, double secs, long long ops)
{
    double mops = (double)ops / secs / 1e6;
    printf("  %-42s %7.3f s  %8.2f M ops/s\n", name, secs, mops);
}

static unsigned long long rng = 0x9E3779B97F4A7C15u;
static unsigned long long rnd(void)
{
    rng = rng * 6364136223846793005u + 1442695040888963407u;
    return rng >> 33;
}

static void bench_vector(void)
{
    clib_vector(int) v;
    int i;
    double t;

    printf("\nvector\n");
    clib_vector_init(&v);

    t = now();
    for (i = 0; i < N_VEC; i++)
        if (clib_vector_push(&v, i) != CLIB_OK) { printf("  OOM\n"); break; }
    report("push (geometric growth)", now() - t, N_VEC);

    t = now();
    {
        long long sum = 0;
        for (i = 0; i < (int)clib_vector_size(&v); i++) sum += v.data[i];
        sink = sum;
    }
    report("sequential read", now() - t, (long long)clib_vector_size(&v));

    /* Pre-reserving removes every regrow: the difference is the growth cost. */
    {
        clib_vector(int) r;
        clib_vector_init(&r);
        clib_vector_reserve_raw((clib_vector_raw *)&r, N_VEC);
        t = now();
        for (i = 0; i < N_VEC; i++) (void)clib_vector_push(&r, i);
        report("push (capacity pre-reserved)", now() - t, N_VEC);
        clib_vector_free(&r);
    }

    clib_vector_free(&v);
}

static void bench_hashmap(void)
{
    clib_hashmap m;
    static int vals[N_HASH];
    char key[32];
    int i;
    double t;

    printf("\nhashmap\n");
    clib_hashmap_init(&m, NULL);

    t = now();
    for (i = 0; i < N_HASH; i++) {
        vals[i] = i;
        snprintf(key, sizeof key, "key-%d", i);
        if (clib_hashmap_put(&m, key, &vals[i]) != CLIB_OK) { printf("  OOM\n"); break; }
    }
    report("insert", now() - t, N_HASH);

    t = now();
    {
        long long hits = 0;
        for (i = 0; i < N_HASH; i++) {
            snprintf(key, sizeof key, "key-%d", i);
            if (clib_hashmap_get(&m, key)) hits++;
        }
        printf("  %-42s %lld/%d hit\n", "lookup (all present)", hits, N_HASH);
    }
    report("lookup", now() - t, N_HASH);

    t = now();
    {
        long long miss = 0;
        for (i = 0; i < N_HASH; i++) {
            snprintf(key, sizeof key, "absent-%d", i);
            if (clib_hashmap_get(&m, key)) miss++;
        }
        if (miss) printf("  (unexpected hits: %lld)\n", miss);
    }
    report("lookup (all absent)", now() - t, N_HASH);

    t = now();
    for (i = 0; i < N_HASH; i++) {
        snprintf(key, sizeof key, "key-%d", i);
        (void)clib_hashmap_remove(&m, key);
    }
    report("remove", now() - t, N_HASH);

    printf("  live=%zu tombstones=%zu (tombstones must stay <= live)\n",
           m.size, m.tombstones);
    clib_hashmap_free(&m);
}

static void bench_sort(void)
{
    static int a[N_SORT], ref[N_SORT];
    int i;
    double t;

    printf("\nsort (%d elements)\n", N_SORT);

    for (i = 0; i < N_SORT; i++) ref[i] = (int)(rnd() % 1000000);

    /* --- random --------------------------------------------------------- */
    memcpy(a, ref, sizeof a);
    t = now();
    clib_sort_quick(a, N_SORT, sizeof a[0], clib_cmp_int, NULL);
    report("quicksort  / random", now() - t, N_SORT);

    memcpy(a, ref, sizeof a);
    t = now();
    (void)clib_sort_merge(a, N_SORT, sizeof a[0], clib_cmp_int, NULL);
    report("mergesort  / random", now() - t, N_SORT);

    /* --- already sorted -------------------------------------------------- */
    memcpy(a, ref, sizeof a);
    clib_sort_quick(a, N_SORT, sizeof a[0], clib_cmp_int, NULL);
    memcpy(ref, a, sizeof ref);          /* keep a sorted copy for reuse */

    t = now();
    clib_sort_quick(a, N_SORT, sizeof a[0], clib_cmp_int, NULL);
    report("quicksort  / already sorted", now() - t, N_SORT);

    memcpy(a, ref, sizeof a);
    t = now();
    (void)clib_sort_merge(a, N_SORT, sizeof a[0], clib_cmp_int, NULL);
    report("mergesort  / already sorted", now() - t, N_SORT);

    /* --- all equal: the case a naive 2-way partition turns quadratic ----- */
    for (i = 0; i < N_SORT; i++) a[i] = 42;
    t = now();
    clib_sort_quick(a, N_SORT, sizeof a[0], clib_cmp_int, NULL);
    report("quicksort  / all equal (3-way)", now() - t, N_SORT);

    for (i = 0; i < N_SORT; i++) a[i] = 42;
    t = now();
    (void)clib_sort_merge(a, N_SORT, sizeof a[0], clib_cmp_int, NULL);
    report("mergesort  / all equal", now() - t, N_SORT);

    /* --- two distinct values --------------------------------------------- */
    for (i = 0; i < N_SORT; i++) a[i] = (int)(i & 1);
    t = now();
    clib_sort_quick(a, N_SORT, sizeof a[0], clib_cmp_int, NULL);
    report("quicksort  / two distinct values", now() - t, N_SORT);

    /* --- insertion sort: quadratic, so keep it small --------------------- */
    {
        enum { SMALL = 20000 };
        for (i = 0; i < SMALL; i++) a[i] = (int)(rnd() % 1000);
        t = now();
        clib_sort_insertion(a, SMALL, sizeof a[0], clib_cmp_int, NULL);
        report("insertsort / random (20k, O(n^2)!)", now() - t, SMALL);
    }
}

static void bench_heap(void)
{
    clib_heap h;
    static int vals[N_HEAP];
    int i;
    double t;

    printf("\nheap\n");
    clib_heap_init(&h, NULL);

    t = now();
    for (i = 0; i < N_HEAP; i++) {
        vals[i] = i;
        (void)clib_heap_push(&h, (long long)(rnd() % 1000000), &vals[i]);
    }
    report("push", now() - t, N_HEAP);

    t = now();
    while (clib_heap_size(&h)) (void)clib_heap_pop(&h);
    report("pop", now() - t, N_HEAP);

    clib_heap_free(&h);
}

static void bench_graph(void)
{
    clib_graph g;
    long long *d;
    size_t *p;
    const size_t V = 20000, E = 100000;
    size_t i;
    double t;

    printf("\ngraph (%zu vertices, %zu edges)\n", V, E);

    /* Allocate on the heap: 20k of these would not fit on the stack twice. */
    d = malloc(V * sizeof *d);
    p = malloc(V * sizeof *p);
    if (!d || !p) { printf("  OOM, skipping\n"); free(d); free(p); return; }

    if (clib_graph_init(&g, V, NULL) != CLIB_OK) { printf("  OOM\n"); free(d); free(p); return; }
    for (i = 0; i < E; i++) {
        size_t u = (size_t)(rnd() % V), v = (size_t)(rnd() % V);
        if (clib_graph_add_edge(&g, u, v, (long long)(rnd() % 1000)) != CLIB_OK) break;
    }

    t = now();
    (void)clib_graph_bfs(&g, 0, d);
    report("BFS", now() - t, (long long)V);

    t = now();
    (void)clib_graph_dijkstra(&g, 0, d, p);
    report("Dijkstra", now() - t, (long long)V);

    clib_graph_free(&g);
    free(d);
    free(p);
}

static void bench_strbuf(void)
{
    clib_strbuf b;
    int i;
    double t;

    printf("\nstrbuf\n");
    if (clib_strbuf_init(&b, NULL) != CLIB_OK) return;

    t = now();
    for (i = 0; i < 200000; i++)
        if (clib_strbuf_append(&b, "x") != CLIB_OK) break;
    report("append 1 char x 200k", now() - t, 200000);

    clib_strbuf_clear(&b);

    t = now();
    for (i = 0; i < 200000; i++)
        if (clib_strbuf_appendf(&b, "%d", i) != CLIB_OK) break;
    report("appendf x 200k", now() - t, 200000);

    clib_strbuf_free(&b);
}

int main(void)
{
    printf("clib benchmarks (lower is better)\n");
    bench_vector();
    bench_hashmap();
    bench_sort();
    bench_heap();
    bench_graph();
    bench_strbuf();
    printf("\ndone\n");
    return 0;
}
