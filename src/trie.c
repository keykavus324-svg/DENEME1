#include <clib/trie.h>
#include <string.h>

#define CLIB_TRIE_MIN_KIDS 4u

static clib_trie_node *node_new(clib_trie *t)
{
    clib_trie_node *n = clib_alloc(t->alloc, sizeof *n);
    if (!n) return NULL;

    n->kids = NULL;
    n->nkids = n->ckids = 0;
    n->terminal = 0;
    n->value = NULL;
    t->nodes++;
    return n;
}

/** Binary search over the sorted child array. */
static clib_trie_node *child_find(const clib_trie_node *n, char ch)
{
    size_t lo = 0, hi = n->nkids;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (n->kids[mid].ch == ch) return n->kids[mid].node;
        if (n->kids[mid].ch < ch) lo = mid + 1;
        else                      hi = mid;
    }
    return NULL;
}

/** Insert a new edge, keeping the array sorted. */
static clib_trie_node *child_add(clib_trie *t, clib_trie_node *n, char ch)
{
    clib_trie_node *kid;
    size_t pos = 0;

    while (pos < n->nkids && n->kids[pos].ch < ch) pos++;
    if (pos < n->nkids && n->kids[pos].ch == ch) return n->kids[pos].node;

    if (n->nkids == n->ckids) {
        size_t cap = n->ckids ? n->ckids * 2 : CLIB_TRIE_MIN_KIDS;
        clib_trie_child *p = clib_realloc(t->alloc, n->kids, cap * sizeof *p);
        if (!p) return NULL;
        n->kids = p;
        n->ckids = cap;
    }

    kid = node_new(t);
    if (!kid) return NULL;

    if (pos < n->nkids)
        memmove(n->kids + pos + 1, n->kids + pos,
                (n->nkids - pos) * sizeof *n->kids);

    n->kids[pos].ch = ch;
    n->kids[pos].node = kid;
    n->nkids++;
    return kid;
}

clib_status clib_trie_init(clib_trie *t, const clib_allocator *a)
{
    if (!t) return CLIB_ERR_INVAL;
    t->root = NULL;
    t->size = t->nodes = 0;
    t->alloc = a;

    t->root = node_new(t);
    return t->root ? CLIB_OK : CLIB_ERR_NOMEM;
}

clib_status clib_trie_put(clib_trie *t, const char *key, void *value)
{
    clib_trie_node *n;
    size_t i;

    if (!t || !key) return CLIB_ERR_INVAL;

    for (n = t->root, i = 0; key[i]; i++) {
        clib_trie_node *next = child_find(n, key[i]);
        if (!next) {
            next = child_add(t, n, key[i]);
            if (!next) return CLIB_ERR_NOMEM;
        }
        n = next;
    }

    if (!n->terminal) t->size++;
    n->terminal = 1;
    n->value = value;
    return CLIB_OK;
}

void *clib_trie_get(const clib_trie *t, const char *key)
{
    const clib_trie_node *n;
    size_t i;

    if (!t || !key) return NULL;
    for (n = t->root, i = 0; key[i]; i++) {
        n = child_find(n, key[i]);
        if (!n) return NULL;
    }
    return n->terminal ? n->value : NULL;
}

int clib_trie_has(const clib_trie *t, const char *key)
{
    const clib_trie_node *n;
    size_t i;

    if (!t || !key) return 0;
    for (n = t->root, i = 0; key[i]; i++) {
        n = child_find(n, key[i]);
        if (!n) return 0;
    }
    return n->terminal;
}

void *clib_trie_remove(clib_trie *t, const char *key)
{
    clib_trie_node **path;
    clib_trie_node *n;
    size_t len, i, depth = 0;
    void *v;

    if (!t || !key || !t->root) return NULL;
    len = strlen(key);

    /* Record the path so pruning can walk back up without recursing. */
    path = clib_alloc(t->alloc, (len + 2) * sizeof *path);
    if (!path) return NULL;

    n = t->root;
    path[depth++] = n;
    for (i = 0; key[i]; i++) {
        n = child_find(n, key[i]);
        if (!n) { clib_free(t->alloc, path); return NULL; }
        path[depth++] = n;
    }

    if (!n->terminal) { clib_free(t->alloc, path); return NULL; }

    v = n->value;
    n->terminal = 0;
    n->value = NULL;
    t->size--;

    /* Prune the deepest childless, non-terminal nodes. The root is kept. */
    for (i = depth; i-- > 1; ) {
        clib_trie_node *parent = path[i - 1];
        clib_trie_node *cur = path[i];
        size_t k;

        if (cur->nkids || cur->terminal) break;

        for (k = 0; k < parent->nkids; k++) {
            if (parent->kids[k].node != cur) continue;
            memmove(parent->kids + k, parent->kids + k + 1,
                    (parent->nkids - k - 1) * sizeof *parent->kids);
            parent->nkids--;
            break;
        }
        clib_free(t->alloc, cur->kids);
        clib_free(t->alloc, cur);
        if (t->nodes) t->nodes--;
    }

    clib_free(t->alloc, path);
    return v;
}

size_t clib_trie_size(const clib_trie *t)
{
    return t ? t->size : 0;
}

/** Longest root-to-leaf path below @p n, in characters. */
static size_t subtree_depth(const clib_trie_node *n)
{
    size_t best = 1, i;

    /* Iterative: key length is bounded, but a deep chain is still input-driven. */
    for (i = 0; i < n->nkids; i++) {
        size_t d = 1 + subtree_depth(n->kids[i].node);
        if (d > best) best = d;
    }
    return best;
}

/*
 * Depth here is bounded by the longest key, not by the number of keys, so
 * recursion is safe - the tree can hold millions of short words and this still
 * only goes as deep as the longest one.
 */
static void walk(const clib_trie_node *n, char *buf, size_t depth,
                 clib_trie_visit_fn fn, void *ctx, size_t *found)
{
    size_t i;

    if (n->terminal) {
        buf[depth] = '\0';
        if (fn) fn(buf, n->value, ctx);
        (*found)++;
    }

    /* Children are stored sorted, so this visit order is lexicographic. */
    for (i = 0; i < n->nkids; i++) {
        buf[depth] = n->kids[i].ch;
        walk(n->kids[i].node, buf, depth + 1, fn, ctx, found);
    }
}

size_t clib_trie_collect(const clib_trie *t, const char *prefix,
                         clib_trie_visit_fn fn, void *ctx)
{
    const clib_trie_node *n;
    char *buf;
    size_t found = 0, i, plen, need;

    if (!t || !t->root) return 0;
    if (!prefix) prefix = "";

    for (n = t->root, i = 0; prefix[i]; i++) {
        n = child_find(n, prefix[i]);
        if (!n) return 0;
    }
    plen = i;

    /* Size the scratch buffer from the actual subtree depth, not a guess. */
    need = plen + subtree_depth(n) + 1;
    buf = clib_alloc(t->alloc, need);
    if (!buf) return 0;
    memcpy(buf, prefix, plen);

    walk(n, buf, plen, fn, ctx, &found);
    clib_free(t->alloc, buf);
    return found;
}

/** Free everything below @p n, and @p n itself unless it is the root. */
static void drop_subtree(clib_trie *t, clib_trie_node *n, int keep_self)
{
    size_t i;
    for (i = 0; i < n->nkids; i++)
        drop_subtree(t, n->kids[i].node, 0);

    clib_free(t->alloc, n->kids);
    n->kids = NULL;
    n->nkids = n->ckids = 0;
    n->terminal = 0;
    n->value = NULL;

    if (!keep_self) {
        clib_free(t->alloc, n);
        if (t->nodes) t->nodes--;
    }
}

void clib_trie_clear(clib_trie *t)
{
    if (!t || !t->root) return;
    drop_subtree(t, t->root, 1);
    t->size = 0;
}

void clib_trie_free(clib_trie *t)
{
    if (!t || !t->root) return;
    drop_subtree(t, t->root, 0);
    t->root = NULL;
    t->size = 0;
}
