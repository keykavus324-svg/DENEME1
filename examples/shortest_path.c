/**
 * @file shortest_path.c
 * @brief Dijkstra on a weighted grid, with the path reconstructed.
 *
 * Exercises clib_graph, clib_heap (inside Dijkstra) and clib_dlist for the
 * predecessor chain.
 *
 *     ./shortest_path 12 8 2026
 */
#include <clib/common.h>
#include <clib/dlist.h>
#include <clib/graph.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned long long rng_state;
static unsigned long long rnd(void)
{
    rng_state = rng_state * 6364136223846793005u + 1442695040888963407u;
    return rng_state >> 33;
}

static size_t vid(size_t row, size_t col, size_t cols)
{
    return row * cols + col;
}

int main(int argc, char **argv)
{
    size_t rows = 10, cols = 10;
    size_t n, start, goal, i;
    clib_graph g;
    long long *dist;
    size_t *prev;
    unsigned long long seed = 1;

    if (argc > 3) {
        rows = (size_t)strtoul(argv[1], NULL, 10);
        cols = (size_t)strtoul(argv[2], NULL, 10);
    }
    if (argc > 3) seed = strtoull(argv[3], NULL, 10);
    if (rows == 0 || cols == 0) {
        fprintf(stderr, "usage: %s [rows cols seed]\n", argv[0]);
        return 2;
    }

    n = rows * cols;
    start = vid(0, 0, cols);
    goal = vid(rows - 1, cols - 1, cols);

    if (clib_graph_init(&g, n, NULL) != CLIB_OK) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    dist = malloc(n * sizeof *dist);
    prev = malloc(n * sizeof *prev);
    if (!dist || !prev) { fprintf(stderr, "out of memory\n"); return 1; }

    /* Random weights in 1..9; Dijkstra needs them non-negative. */
    rng_state = seed * 2654435761u + 1u;
    {
        size_t r, c;
        for (r = 0; r < rows; r++)
            for (c = 0; c < cols; c++) {
                size_t here = vid(r, c, cols);
                if (c + 1 < cols) {
                    long long w = (long long)(rnd() % 9) + 1;
                    if (clib_graph_add_edge_undirected(&g, here, vid(r, c + 1, cols), w) != CLIB_OK)
                        goto oom;
                }
                if (r + 1 < rows) {
                    long long w = (long long)(rnd() % 9) + 1;
                    if (clib_graph_add_edge_undirected(&g, here, vid(r + 1, c, cols), w) != CLIB_OK)
                        goto oom;
                }
            }
    }

    if (clib_graph_dijkstra(&g, start, dist, prev) != CLIB_OK) goto oom;

    if (dist[goal] == CLIB_GRAPH_UNREACHABLE) {
        printf("goal unreachable\n");
    } else {
        /* Walk the predecessor chain backwards, then reverse it with a dlist. */
        clib_dlist path;
        size_t at = goal;

        clib_dlist_init(&path, NULL);
        while (1) {
            size_t *slot = malloc(sizeof *slot);
            if (!slot) goto oom;
            *slot = at;
            if (clib_dlist_push_front(&path, slot) != CLIB_OK) { free(slot); goto oom; }
            if (at == start) break;
            at = prev[at];
        }

        printf("shortest path: %zu -> %zu, cost %lld, %zu steps\n\n",
               start, goal, dist[goal], path.size);

        for (i = 0; i < rows; i++) {
            size_t c;
            for (c = 0; c < cols; c++) {
                int on_path = 0;
                clib_dlist_node *node;
                for (node = path.head; node; node = node->next)
                    if (*(size_t *)node->value == vid(i, c, cols)) { on_path = 1; break; }
                putchar(on_path ? '#' : '.');
            }
            putchar('\n');
        }

        {
            clib_dlist_node *node = path.head, *next;
            (void)next;
            for (node = path.head; node; node = node->next) free(node->value);
        }
        clib_dlist_free(&path);
    }

    clib_graph_free(&g);
    free(dist);
    free(prev);
    return 0;

oom:
    fprintf(stderr, "out of memory\n");
    clib_graph_free(&g);
    free(dist);
    free(prev);
    return 1;
}
