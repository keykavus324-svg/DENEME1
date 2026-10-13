/**
 * @file graph.h
 * @brief Weighted directed graph as an adjacency list, with BFS, DFS and Dijkstra.
 *
 * Vertices are dense indices `0 .. n-1`. Edges are stored as singly linked
 * lists hanging off each vertex, so memory is O(n + m) rather than O(n^2).
 *
 * The traversals are built on top of the other containers in this library:
 * BFS uses ::clib_queue, DFS uses ::clib_stack, and Dijkstra uses ::clib_heap.
 * That is deliberate - it keeps each traversal to the part that is actually
 * interesting.
 */
#ifndef CLIB_GRAPH_H
#define CLIB_GRAPH_H

#include <clib/common.h>
#include <clib/uf.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Sentinel distance for vertices that cannot be reached. */
#define CLIB_GRAPH_UNREACHABLE (-1)

/** One outgoing arc. */
typedef struct clib_graph_edge {
    size_t to;                    /**< Target vertex index. */
    long long weight;             /**< May be negative for BFS/DFS; not for Dijkstra. */
    struct clib_graph_edge *next; /**< Next arc from the same source. */
} clib_graph_edge;

/** A weighted directed graph. */
typedef struct clib_graph {
    clib_graph_edge **adj;        /**< Array of n list heads. */
    size_t n;                     /**< Vertex count. */
    size_t m;                     /**< Edge count. */
    const clib_allocator *alloc;
} clib_graph;

/** Callback for clib_graph_dfs(); vertices are reported in pre-order. */
typedef void (*clib_graph_visit_fn)(size_t vertex, void *ctx);

/** Allocate an empty graph with @p n vertices and no edges. */
clib_status clib_graph_init(clib_graph *g, size_t n, const clib_allocator *a);

/**
 * Add an arc u -> v with the given weight. Duplicate arcs are allowed and are
 * treated as separate parallel edges.
 */
clib_status clib_graph_add_edge(clib_graph *g, size_t u, size_t v, long long w);

/** Convenience: add both u -> v and v -> u. */
clib_status clib_graph_add_edge_undirected(clib_graph *g, size_t u, size_t v, long long w);

size_t clib_graph_vertices(const clib_graph *g);
size_t clib_graph_edges(const clib_graph *g);

/**
 * Breadth-first search from @p src. Fills @p dist (array of @p n) with the hop
 * count to each vertex, or CLIB_GRAPH_UNREACHABLE. O(n + m).
 */
clib_status clib_graph_bfs(const clib_graph *g, size_t src, long long *dist);

/**
 * Depth-first search from @p src, reporting each vertex once, in pre-order.
 * O(n + m). The walk is iterative, so deep graphs cannot overflow the stack.
 */
clib_status clib_graph_dfs(const clib_graph *g, size_t src,
                           clib_graph_visit_fn fn, void *ctx);

/**
 * Dijkstra's shortest paths from @p src.
 *
 * @p dist receives the total weight (or CLIB_GRAPH_UNREACHABLE), @p prev the
 * predecessor vertex (or `src` itself for unreachable/start vertices). Both are
 * caller-owned arrays of @p n.
 *
 * Weights must be non-negative; a negative arc makes the result meaningless.
 * O((n + m) log n).
 */
clib_status clib_graph_dijkstra(const clib_graph *g, size_t src,
                                long long *dist, size_t *prev);

/**
 * Topological order of a directed acyclic graph (Kahn's algorithm).
 *
 * @p out receives the vertices in dependency order and must hold @p n entries.
 * @p out_n receives how many were written.
 *
 * Returns CLIB_OK when every vertex was emitted, or CLIB_ERR_INVAL when the
 * graph has a cycle - in which case @p out is partial and @p out_n is short.
 * O(n + m).
 */
clib_status clib_graph_topo_sort(const clib_graph *g, size_t *out, size_t *out_n);

/** One edge of a minimum spanning tree. */
typedef struct clib_graph_mst_edge {
    size_t u;             /**< Lower-numbered endpoint. */
    size_t v;             /**< Higher-numbered endpoint. */
    long long weight;     /**< Arc weight. */
} clib_graph_mst_edge;

/**
 * Minimum spanning forest by Kruskal's algorithm.
 *
 * @p out must hold at least `n - 1` entries; @p out_n receives how many were
 * written. A connected graph yields exactly `n - 1` edges; a disconnected one
 * yields `n - components` and this is still the correct answer (a forest).
 *
 * Built on ::clib_uf for the cycle test and clib_sort_quick for the edge order.
 * O(m log m).
 */
clib_status clib_graph_mst(const clib_graph *g, clib_graph_mst_edge *out, size_t *out_n);

/**
 * Single-source shortest paths allowing **negative** arc weights.
 *
 * Slower than clib_graph_dijkstra() but it is defined on graphs that Dijkstra
 * is not. Returns CLIB_ERR_INVAL when a negative cycle is reachable from
 * @p src, because then no shortest path exists and the answer would be
 * meaningless. Unreachable vertices get CLIB_GRAPH_UNREACHABLE. O(n*m).
 */
clib_status clib_graph_bellman_ford(const clib_graph *g, size_t src,
                                    long long *dist, size_t *prev);

/** Free every arc and the adjacency array. */
void clib_graph_free(clib_graph *g);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_GRAPH_H */
