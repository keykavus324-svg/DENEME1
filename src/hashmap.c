#include <clib/hashmap.h>
#include <string.h>

#define CLIB_HM_MIN_CAP 16u

/* Non-NULL sentinel marking a slot that was used and then deleted. */
static char clib_hm_tombstone_storage;
#define CLIB_HM_TOMBSTONE (&clib_hm_tombstone_storage)

size_t clib_hash_str(const char *s)
{
    size_t h = (size_t)0xcbf29ce484222325u; /* FNV offset basis (64-bit) */
    while (s && *s) {
        h ^= (unsigned char)*s++;
        h *= (size_t)0x100000001b3u;        /* FNV prime */
    }
    return h;
}

void clib_hashmap_init(clib_hashmap *m, const clib_allocator *a)
{
    if (!m) return;
    m->slots = NULL;
    m->capacity = m->size = m->tombstones = 0;
    m->alloc = a;
}

static int key_is_live(const char *k)
{
    return k && k != CLIB_HM_TOMBSTONE;
}

/** Find the slot holding @p key, or the first empty slot where it would go. */
static size_t probe(const clib_hashmap *m, const char *key, int *found)
{
    size_t mask = m->capacity - 1;
    size_t i = clib_hash_str(key) & mask;
    size_t first_free = (size_t)-1;

    *found = 0;
    for (;;) {
        const char *k = m->slots[i].key;

        if (!k) {
            if (first_free != (size_t)-1) return first_free;
            return i;
        }
        if (k == CLIB_HM_TOMBSTONE) {
            if (first_free == (size_t)-1) first_free = i;
        } else if (strcmp(k, key) == 0) {
            *found = 1;
            return i;
        }
        i = (i + 1) & mask;
    }
}

static clib_status rehash(clib_hashmap *m, size_t newcap)
{
    clib_hashmap_entry *slots;
    clib_hashmap fresh;
    size_t i;

    if (newcap > ((size_t)-1) / sizeof *slots) return CLIB_ERR_NOMEM;

    slots = clib_alloc(m->alloc, newcap * sizeof *slots);
    if (!slots) return CLIB_ERR_NOMEM;
    for (i = 0; i < newcap; i++) { slots[i].key = NULL; slots[i].value = NULL; }

    fresh.slots = slots;
    fresh.capacity = newcap;
    fresh.size = 0;
    fresh.tombstones = 0;
    fresh.alloc = m->alloc;

    for (i = 0; i < m->capacity; i++) {
        int found;
        size_t dst;
        if (!key_is_live(m->slots[i].key)) continue;

        dst = probe(&fresh, m->slots[i].key, &found);
        fresh.slots[dst].key = m->slots[i].key;   /* move ownership */
        fresh.slots[dst].value = m->slots[i].value;
        fresh.size++;
    }

    clib_free(m->alloc, m->slots);
    *m = fresh;
    return CLIB_OK;
}

clib_status clib_hashmap_put(clib_hashmap *m, const char *key, void *value)
{
    int found;
    size_t i;
    char *copy;

    if (!m || !key) return CLIB_ERR_INVAL;

    /* Grow on live entries + tombstones, so delete-heavy maps don't rot. */
    if (m->capacity == 0 ||
        (double)(m->size + m->tombstones + 1) / (double)m->capacity > CLIB_HASHMAP_MAX_LOAD) {
        clib_status s = rehash(m, m->capacity ? m->capacity * 2 : CLIB_HM_MIN_CAP);
        if (s != CLIB_OK) return s;
    }

    i = probe(m, key, &found);

    if (found) {
        m->slots[i].value = value;
        return CLIB_OK;
    }

    {
        size_t n = strlen(key) + 1;
        copy = clib_alloc(m->alloc, n);
        if (!copy) return CLIB_ERR_NOMEM;
        memcpy(copy, key, n);
    }

    if (m->slots[i].key == CLIB_HM_TOMBSTONE) m->tombstones--;
    m->slots[i].key = copy;
    m->slots[i].value = value;
    m->size++;
    return CLIB_OK;
}

void *clib_hashmap_get(const clib_hashmap *m, const char *key)
{
    int found;
    size_t i;

    if (!m || !key || m->capacity == 0) return NULL;
    i = probe(m, key, &found);
    return found ? m->slots[i].value : NULL;
}

int clib_hashmap_has(const clib_hashmap *m, const char *key)
{
    int found;
    if (!m || !key || m->capacity == 0) return 0;
    (void)probe(m, key, &found);
    return found;
}

const char *clib_hashmap_key(const clib_hashmap *m, const char *key)
{
    int found;
    size_t i;

    if (!m || !key || m->capacity == 0) return NULL;
    i = probe(m, key, &found);
    return found ? m->slots[i].key : NULL;
}

clib_status clib_hashmap_remove(clib_hashmap *m, const char *key)
{
    int found;
    size_t i;

    if (!m || !key || m->capacity == 0) return CLIB_ERR_NOTFOUND;
    i = probe(m, key, &found);
    if (!found) return CLIB_ERR_NOTFOUND;

    clib_free(m->alloc, m->slots[i].key);
    m->slots[i].key = CLIB_HM_TOMBSTONE;
    m->slots[i].value = NULL;
    m->size--;
    m->tombstones++;

    /* Once deletions dominate, rebuild to clear the tombstones out. */
    if (m->tombstones > m->size && m->capacity > CLIB_HM_MIN_CAP)
        (void)rehash(m, m->capacity);

    return CLIB_OK;
}

size_t clib_hashmap_size(const clib_hashmap *m)
{
    return m ? m->size : 0;
}

void clib_hashmap_foreach(const clib_hashmap *m, clib_hashmap_visit_fn fn, void *ctx)
{
    size_t i;
    if (!m || !fn) return;
    for (i = 0; i < m->capacity; i++)
        if (key_is_live(m->slots[i].key))
            fn(m->slots[i].key, m->slots[i].value, ctx);
}

void clib_hashmap_clear(clib_hashmap *m)
{
    size_t i;
    if (!m) return;
    for (i = 0; i < m->capacity; i++)
        if (key_is_live(m->slots[i].key))
            clib_free(m->alloc, m->slots[i].key);
    clib_free(m->alloc, m->slots);
    m->slots = NULL;
    m->capacity = m->size = m->tombstones = 0;
}

void clib_hashmap_free(clib_hashmap *m)
{
    clib_hashmap_clear(m);
}
