#include <clib/graph.h>
#include <clib/heap.h>
#include <clib/queue.h>
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
