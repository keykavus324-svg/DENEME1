#include <clib/graph.h>
#include <clib/heap.h>
#include <clib/queue.h>
#include <clib/sort.h>
#include <clib/stack.h>

clib_status clib_graph_init(clib_graph *g, size_t n, const clib_allocator *a)
{
    size_t i;

    if (!g) return CLIB_ERR_INVAL;
    g->adj = NULL;
    g->n = g->m = 0;
    g->alloc = a;

    if (n == 0) return CLIB_OK;
    if (n > ((size_t)-1) / sizeof *g->adj) return CLIB_ERR_NOMEM;

    g->adj = clib_alloc(a, n * sizeof *g->adj);
    if (!g->adj) return CLIB_ERR_NOMEM;
    for (i = 0; i < n; i++) g->adj[i] = NULL;

    g->n = n;
    return CLIB_OK;
}

clib_status clib_graph_add_edge(clib_graph *g, size_t u, size_t v, long long w)
{
    clib_graph_edge *e;

    if (!g || u >= g->n || v >= g->n) return CLIB_ERR_INVAL;

    e = clib_alloc(g->alloc, sizeof *e);
    if (!e) return CLIB_ERR_NOMEM;

    e->to = v;
    e->weight = w;
    e->next = g->adj[u];        /* prepend: O(1) */
    g->adj[u] = e;
    g->m++;
    return CLIB_OK;
}

clib_status clib_graph_add_edge_undirected(clib_graph *g, size_t u, size_t v, long long w)
{
    clib_status s = clib_graph_add_edge(g, u, v, w);
    if (s != CLIB_OK) return s;
    return clib_graph_add_edge(g, v, u, w);
}

size_t clib_graph_vertices(const clib_graph *g)
{
    return g ? g->n : 0;
}

size_t clib_graph_edges(const clib_graph *g)
{
    return g ? g->m : 0;
}

clib_status clib_graph_bfs(const clib_graph *g, size_t src, long long *dist)
{
    clib_queue q;
    clib_status s;
    size_t i;
    unsigned char *seen;

    if (!g || !dist || src >= g->n) return CLIB_ERR_INVAL;

    for (i = 0; i < g->n; i++) dist[i] = CLIB_GRAPH_UNREACHABLE;
    if (g->n == 0) return CLIB_OK;

    seen = clib_alloc(g->alloc, g->n * sizeof *seen);
    if (!seen) return CLIB_ERR_NOMEM;
    for (i = 0; i < g->n; i++) seen[i] = 0;

    clib_queue_init(&q, g->alloc);

    dist[src] = 0;
    seen[src] = 1;
    s = clib_queue_enqueue(&q, &dist[src]);   /* enqueue the slot, not a copy */
    if (s != CLIB_OK) { clib_queue_free(&q); clib_free(g->alloc, seen); return s; }

    while (clib_queue_size(&q)) {
        /* Recover the index from the enqueued pointer: no int->ptr casting. */
        size_t u = (size_t)((long long *)clib_queue_peek(&q) - dist);
        (void)clib_queue_dequeue(&q);

        for (clib_graph_edge *e = g->adj[u]; e; e = e->next) {
            if (seen[e->to]) continue;
            seen[e->to] = 1;
            dist[e->to] = dist[u] + 1;
            s = clib_queue_enqueue(&q, &dist[e->to]);
            if (s != CLIB_OK) {
                clib_queue_free(&q);
                clib_free(g->alloc, seen);
                return s;
            }
        }
    }

    clib_queue_free(&q);
    clib_free(g->alloc, seen);
    return CLIB_OK;
}

clib_status clib_graph_dfs(const clib_graph *g, size_t src,
                           clib_graph_visit_fn fn, void *ctx)
{
    clib_stack st;
    clib_status s;
    size_t i;
    unsigned char *seen;
    size_t *ids;

    if (!g || src >= g->n) return CLIB_ERR_INVAL;
    if (!fn) return CLIB_OK;
    if (g->n == 0) return CLIB_OK;

    seen = clib_alloc(g->alloc, g->n * sizeof *seen);
    ids  = clib_alloc(g->alloc, g->n * sizeof *ids);
    if (!seen || !ids) {
        clib_free(g->alloc, seen);
        clib_free(g->alloc, ids);
        return CLIB_ERR_NOMEM;
    }
    for (i = 0; i < g->n; i++) { seen[i] = 0; ids[i] = i; }

    clib_stack_init(&st, g->alloc);

    s = clib_stack_push(&st, &ids[src]);
    if (s != CLIB_OK) { clib_stack_free(&st); clib_free(g->alloc, seen); clib_free(g->alloc, ids); return s; }

    while (clib_stack_size(&st)) {
        size_t u = (size_t)((size_t *)clib_stack_pop(&st) - ids);
        if (seen[u]) continue;
        seen[u] = 1;
        fn(u, ctx);

        /* Push neighbours in reverse so they pop in adjacency order. */
        for (clib_graph_edge *e = g->adj[u]; e; e = e->next) {
            if (seen[e->to]) continue;
            s = clib_stack_push(&st, &ids[e->to]);
            if (s != CLIB_OK) {
                clib_stack_free(&st);
                clib_free(g->alloc, seen);
                clib_free(g->alloc, ids);
                return s;
            }
        }
    }

    clib_stack_free(&st);
    clib_free(g->alloc, seen);
    clib_free(g->alloc, ids);
    return CLIB_OK;
}

clib_status clib_graph_dijkstra(const clib_graph *g, size_t src,
                                long long *dist, size_t *prev)
{
    clib_heap h;
    clib_status s;
    size_t i;
    unsigned char *done;
    size_t *ids;

    if (!g || !dist || !prev || src >= g->n) return CLIB_ERR_INVAL;
    if (g->n == 0) return CLIB_OK;

    done = clib_alloc(g->alloc, g->n * sizeof *done);
    ids  = clib_alloc(g->alloc, g->n * sizeof *ids);
    if (!done || !ids) {
        clib_free(g->alloc, done);
        clib_free(g->alloc, ids);
        return CLIB_ERR_NOMEM;
    }
    for (i = 0; i < g->n; i++) {
        dist[i] = CLIB_GRAPH_UNREACHABLE;
        prev[i] = src;
        done[i] = 0;
        ids[i] = i;
    }

    clib_heap_init(&h, g->alloc);
    dist[src] = 0;
    s = clib_heap_push(&h, 0, &ids[src]);
    if (s != CLIB_OK) { clib_heap_free(&h); clib_free(g->alloc, done); clib_free(g->alloc, ids); return s; }

    while (clib_heap_size(&h)) {
        size_t u = (size_t)((size_t *)clib_heap_pop(&h) - ids);
        clib_graph_edge *e;

        if (done[u]) continue;      /* stale entry: a better path already settled */
        done[u] = 1;

        for (e = g->adj[u]; e; e = e->next) {
            long long nd = dist[u] + e->weight;
            if (done[e->to]) continue;
            if (dist[e->to] == CLIB_GRAPH_UNREACHABLE || nd < dist[e->to]) {
                dist[e->to] = nd;
                prev[e->to] = u;
                s = clib_heap_push(&h, nd, &ids[e->to]);
                if (s != CLIB_OK) {
                    clib_heap_free(&h);
                    clib_free(g->alloc, done);
                    clib_free(g->alloc, ids);
                    return s;
                }
            }
        }
    }

    clib_heap_free(&h);
    clib_free(g->alloc, done);
    clib_free(g->alloc, ids);
    return CLIB_OK;
}

clib_status clib_graph_topo_sort(const clib_graph *g, size_t *out, size_t *out_n)
{
    size_t *indeg;
    clib_queue q;
    clib_status s = CLIB_OK;
    size_t i, emitted = 0;

    if (!g || !out || !out_n) return CLIB_ERR_INVAL;
    *out_n = 0;
    if (g->n == 0) return CLIB_OK;

    indeg = clib_alloc(g->alloc, g->n * sizeof *indeg);
    if (!indeg) return CLIB_ERR_NOMEM;
    for (i = 0; i < g->n; i++) indeg[i] = 0;

    for (i = 0; i < g->n; i++) {
        clib_graph_edge *e;
        for (e = g->adj[i]; e; e = e->next) indeg[e->to]++;
    }

    clib_queue_init(&q, g->alloc);
    for (i = 0; i < g->n; i++) {
        if (indeg[i]) continue;
        /* Enqueue the slot: the index is recovered by pointer subtraction. */
        s = clib_queue_enqueue(&q, &indeg[i]);
        if (s != CLIB_OK) goto done;
    }

    while (clib_queue_size(&q)) {
        size_t u = (size_t)((size_t *)clib_queue_dequeue(&q) - indeg);
        clib_graph_edge *e;

        out[emitted++] = u;

        for (e = g->adj[u]; e; e = e->next) {
            if (--indeg[e->to] == 0) {
                s = clib_queue_enqueue(&q, &indeg[e->to]);
                if (s != CLIB_OK) goto done;
            }
        }
    }

    /* Short output means something still has an unmet dependency: a cycle. */
    if (emitted != g->n) s = CLIB_ERR_INVAL;

done:
    *out_n = emitted;
    clib_queue_free(&q);
    clib_free(g->alloc, indeg);
    return s;
}

static int cmp_arc(const void *lhs, const void *rhs, void *ctx)
{
    const clib_graph_mst_edge *a = lhs, *b = rhs;
    (void)ctx;
    return (a->weight > b->weight) - (a->weight < b->weight);
}

clib_status clib_graph_mst(const clib_graph *g, clib_graph_mst_edge *out, size_t *out_n)
{
    clib_graph_mst_edge *arcs = NULL;
    clib_uf sets;
    clib_status s;
    size_t i, count = 0, taken = 0;

    if (!g || !out || !out_n) return CLIB_ERR_INVAL;
    *out_n = 0;
    if (g->n < 2) return CLIB_OK;

    if (g->m) {
        arcs = clib_alloc(g->alloc, g->m * sizeof *arcs);
        if (!arcs) return CLIB_ERR_NOMEM;
    }

    for (i = 0; i < g->n && count < g->m; i++) {
        clib_graph_edge *e;
        for (e = g->adj[i]; e && count < g->m; e = e->next) {
            arcs[count].u = i < e->to ? i : e->to;
            arcs[count].v = i < e->to ? e->to : i;
            arcs[count].weight = e->weight;
            count++;
        }
    }

    clib_sort_quick(arcs, count, sizeof arcs[0], cmp_arc, NULL);

    s = clib_uf_init(&sets, g->n, g->alloc);
    if (s != CLIB_OK) { clib_free(g->alloc, arcs); return s; }

    for (i = 0; i < count; i++) {
        if (clib_uf_connected(&sets, arcs[i].u, arcs[i].v)) continue;  /* cycle */
        if (clib_uf_union(&sets, arcs[i].u, arcs[i].v) != CLIB_OK) continue;
        out[taken++] = arcs[i];
        if (taken == g->n - 1) break;         /* spanning tree complete */
    }

    *out_n = taken;
    clib_uf_free(&sets);
    clib_free(g->alloc, arcs);
    return CLIB_OK;
}

clib_status clib_graph_bellman_ford(const clib_graph *g, size_t src,
                                    long long *dist, size_t *prev)
{
    size_t i, iter;

    if (!g || !dist || !prev || src >= g->n) return CLIB_ERR_INVAL;

    for (i = 0; i < g->n; i++) {
        dist[i] = CLIB_GRAPH_UNREACHABLE;
        prev[i] = src;
    }
    dist[src] = 0;

    /* n-1 passes are enough for any graph without a negative cycle. */
    for (iter = 1; iter < g->n; iter++) {
        int changed = 0;
        for (i = 0; i < g->n; i++) {
            clib_graph_edge *e;
            if (dist[i] == CLIB_GRAPH_UNREACHABLE) continue;
            for (e = g->adj[i]; e; e = e->next) {
                long long nd = dist[i] + e->weight;
                if (dist[e->to] == CLIB_GRAPH_UNREACHABLE || nd < dist[e->to]) {
                    dist[e->to] = nd;
                    prev[e->to] = i;
                    changed = 1;
                }
            }
        }
        if (!changed) break;                  /* settled early */
    }

    /* One more pass still relaxing means a negative cycle is reachable. */
    for (i = 0; i < g->n; i++) {
        clib_graph_edge *e;
        if (dist[i] == CLIB_GRAPH_UNREACHABLE) continue;
        for (e = g->adj[i]; e; e = e->next)
            if (dist[e->to] == CLIB_GRAPH_UNREACHABLE ||
                dist[i] + e->weight < dist[e->to])
                return CLIB_ERR_INVAL;
    }
    return CLIB_OK;
}

void clib_graph_free(clib_graph *g)
{
    size_t i;
    if (!g) return;
    for (i = 0; i < g->n; i++) {
        clib_graph_edge *e = g->adj ? g->adj[i] : NULL;
        while (e) {
            clib_graph_edge *next = e->next;
            clib_free(g->alloc, e);
            e = next;
        }
    }
    clib_free(g->alloc, g->adj);
    g->adj = NULL;
    g->n = g->m = 0;
}
