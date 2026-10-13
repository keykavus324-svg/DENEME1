#include <clib/bst.h>
#include <string.h>

typedef struct clib_bst_node {
    const char *key;              /* borrowed */
    void *value;                  /* borrowed */
    struct clib_bst_node *left;
    struct clib_bst_node *right;
} clib_bst_node;

void clib_bst_init(clib_bst *t, const clib_allocator *a)
{
    if (!t) return;
    t->root = NULL;
    t->size = 0;
    t->alloc = a;
}

static clib_bst_node **find_slot(clib_bst_node **link, const char *key)
{
    while (*link) {
        int c = strcmp(key, (*link)->key);
        if (c == 0) return link;
        link = (c < 0) ? &(*link)->left : &(*link)->right;
    }
    return link;
}

clib_status clib_bst_put(clib_bst *t, const char *key, void *value)
{
    clib_bst_node **slot;

    if (!t || !key) return CLIB_ERR_INVAL;

    slot = find_slot(&t->root, key);
    if (*slot) {
        (*slot)->value = value;
        return CLIB_OK;
    }

    *slot = clib_alloc(t->alloc, sizeof **slot);
    if (!*slot) return CLIB_ERR_NOMEM;

    (*slot)->key = key;
    (*slot)->value = value;
    (*slot)->left = (*slot)->right = NULL;
    t->size++;
    return CLIB_OK;
}

void *clib_bst_get(const clib_bst *t, const char *key)
{
    clib_bst_node *n;

    if (!t || !key) return NULL;
    for (n = t->root; n; ) {
        int c = strcmp(key, n->key);
        if (c == 0) return n->value;
        n = (c < 0) ? n->left : n->right;
    }
    return NULL;
}

clib_status clib_bst_remove(clib_bst *t, const char *key)
{
    clib_bst_node **slot, *n;

    if (!t || !key) return CLIB_ERR_INVAL;

    slot = find_slot(&t->root, key);
    if (!*slot) return CLIB_ERR_NOTFOUND;
    n = *slot;

    if (n->left && n->right) {
        /* Two children: promote the in-order successor (min of right subtree). */
        clib_bst_node **succ = &n->right;
        while ((*succ)->left) succ = &(*succ)->left;

        n->key = (*succ)->key;
        n->value = (*succ)->value;

        n = *succ;
        *succ = n->right;
    } else {
        *slot = n->left ? n->left : n->right;
    }

    clib_free(t->alloc, n);
    t->size--;
    return CLIB_OK;
}

size_t clib_bst_size(const clib_bst *t)
{
    return t ? t->size : 0;
}

static size_t height_of(const clib_bst_node *n)
{
    size_t l, r;
    if (!n) return 0;
    l = height_of(n->left);
    r = height_of(n->right);
    return 1 + (l > r ? l : r);
}

size_t clib_bst_height(const clib_bst *t)
{
    return t ? height_of(t->root) : 0;
}

static void walk(const clib_bst_node *n, clib_bst_visit_fn fn, void *ctx)
{
    if (!n) return;
    walk(n->left, fn, ctx);
    fn(n->key, n->value, ctx);
    walk(n->right, fn, ctx);
}

void clib_bst_foreach(const clib_bst *t, clib_bst_visit_fn fn, void *ctx)
{
    if (!t || !fn) return;
    walk(t->root, fn, ctx);
}

static void drop(clib_bst *t, clib_bst_node *n)
{
    if (!n) return;
    drop(t, n->left);
    drop(t, n->right);
    clib_free(t->alloc, n);
}

void clib_bst_clear(clib_bst *t)
{
    if (!t) return;
    drop(t, t->root);
    t->root = NULL;
    t->size = 0;
}

void clib_bst_free(clib_bst *t)
{
    clib_bst_clear(t);
}
