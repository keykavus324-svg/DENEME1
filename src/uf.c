#include <clib/uf.h>

clib_status clib_uf_init(clib_uf *u, size_t n, const clib_allocator *a)
{
    size_t i;

    if (!u) return CLIB_ERR_INVAL;
    u->parent = u->rank = u->set_size = NULL;
    u->n = 0;
    u->sets = 0;
    u->alloc = a;

    if (n == 0) return CLIB_OK;
    if (n > ((size_t)-1) / (3 * sizeof *u->parent)) return CLIB_ERR_NOMEM;

    /* One block for all three arrays: a single allocation, one free. */
    u->parent = clib_alloc(a, 3 * n * sizeof *u->parent);
    if (!u->parent) return CLIB_ERR_NOMEM;
    u->rank = u->parent + n;
    u->set_size = u->rank + n;

    for (i = 0; i < n; i++) {
        u->parent[i] = i;
        u->rank[i] = 0;
        u->set_size[i] = 1;
    }
    u->n = n;
    u->sets = n;
    return CLIB_OK;
}

/*
 * Iterative two-pass path compression: walk up to the root, then walk back
 * down pointing every node straight at it.
 *
 * The recursive version is shorter but a deep chain - which is exactly what a
 * long run of unions without compression produces - can overflow the stack.
 */
size_t clib_uf_find(clib_uf *u, size_t x)
{
    size_t root, walk, next;

    if (!u || x >= u->n) return u ? u->n : 0;

    root = x;
    while (u->parent[root] != root) root = u->parent[root];

    walk = x;
    while (u->parent[walk] != walk) {
        next = u->parent[walk];
        u->parent[walk] = root;
        walk = next;
    }
    return root;
}

clib_status clib_uf_union(clib_uf *u, size_t a, size_t b)
{
    size_t ra, rb;

    if (!u) return CLIB_ERR_INVAL;
    if (a >= u->n || b >= u->n) return CLIB_ERR_INVAL;

    ra = clib_uf_find(u, a);
    rb = clib_uf_find(u, b);
    if (ra == rb) return CLIB_OK;                 /* already together */

    /* Union by rank: hang the shorter tree under the taller one. */
    if (u->rank[ra] < u->rank[rb]) { size_t t = ra; ra = rb; rb = t; }
    else if (u->rank[ra] == u->rank[rb]) u->rank[ra]++;

    u->parent[rb] = ra;
    u->set_size[ra] += u->set_size[rb];
    u->set_size[rb] = 0;
    u->sets--;
    return CLIB_OK;
}

int clib_uf_connected(const clib_uf *u, size_t a, size_t b)
{
    size_t ra, rb;

    if (!u || a >= u->n || b >= u->n) return 0;
    if (a == b) return 1;

    /* Read-only: find() compresses, so walk the chains by hand instead. */
    ra = a; while (u->parent[ra] != ra) ra = u->parent[ra];
    rb = b; while (u->parent[rb] != rb) rb = u->parent[rb];
    return ra == rb;
}

size_t clib_uf_size_of(const clib_uf *u, size_t x)
{
    size_t r;
    if (!u || x >= u->n) return 0;
    r = x;
    while (u->parent[r] != r) r = u->parent[r];
    return u->set_size[r];
}

size_t clib_uf_sets(const clib_uf *u)
{
    return u ? u->sets : 0;
}

size_t clib_uf_count(const clib_uf *u)
{
    return u ? u->n : 0;
}

void clib_uf_reset(clib_uf *u)
{
    size_t i;
    if (!u) return;
    for (i = 0; i < u->n; i++) {
        u->parent[i] = i;
        u->rank[i] = 0;
        u->set_size[i] = 1;
    }
    u->sets = u->n;
}

void clib_uf_free(clib_uf *u)
{
    if (!u) return;
    clib_free(u->alloc, u->parent);
    u->parent = u->rank = u->set_size = NULL;
    u->n = u->sets = 0;
}
