/**
 * @file wordcount.c
 * @brief Count word frequencies on stdin and print the most common ones.
 *
 * Exercises clib_strbuf (tokenising), clib_hashmap (counting) and
 * clib_sort_merge (stable ordering by count, then alphabetically).
 *
 *     printf "the cat the hat sat on the mat" | ./wordcount 5
 */
#include <clib/common.h>
#include <clib/hashmap.h>
#include <clib/sort.h>
#include <clib/strutil.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct word_count {
    const char *word;
    size_t count;
} word_count;

static int cmp_by_count_desc(const void *lhs, const void *rhs, void *ctx)
{
    const word_count *a = lhs, *b = rhs;
    (void)ctx;
    if (a->count != b->count)
        return (a->count > b->count) ? -1 : 1;      /* most frequent first */
    return strcmp(a->word, b->word);                /* then alphabetical */
}

/* clib_hashmap_foreach() takes no user data, so the sink is file-scope. */
static word_count *g_out;
static size_t g_used;

static void collect(const char *key, void *value, void *ctx)
{
    (void)ctx;
    g_out[g_used].word = key;
    g_out[g_used].count = *(const size_t *)value;
    g_used++;
}

/**
 * The map copies keys but only borrows values, so the counters we allocated
 * still belong to us. Nothing else would free them.
 */
static void free_counter(const char *key, void *value, void *ctx)
{
    (void)key; (void)ctx;
    free(value);
}

static int add_word(clib_hashmap *map, const char *word)
{
    size_t *seen = clib_hashmap_get(map, word);

    if (seen) { (*seen)++; return 0; }

    {
        /*
         * No key allocation here on purpose: clib_hashmap_put() copies the key
         * itself, so handing it the caller's buffer is enough. (An earlier
         * version malloc'd a copy as well and leaked it - caught by ASan.)
         * The counter, on the other hand, is only borrowed by the map, so it
         * is ours to allocate and ours to free.
         */
        size_t *ctr = malloc(sizeof *ctr);
        if (!ctr) return -1;
        *ctr = 1;

        if (clib_hashmap_put(map, word, ctr) != CLIB_OK) {
            free(ctr);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    clib_hashmap map;
    clib_strbuf tok;
    word_count *all = NULL;
    long limit = 10;
    int ch;

    if (argc > 1) {
        char *end;
        limit = strtol(argv[1], &end, 10);
        if (*end != '\0' || limit <= 0) {
            fprintf(stderr, "usage: %s [how-many]\n", argv[0]);
            return 2;
        }
    }

    /* clib_hashmap_init() cannot fail: the table is allocated lazily. */
    clib_hashmap_init(&map, NULL);
    if (clib_strbuf_init(&tok, NULL) != CLIB_OK) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    while ((ch = getchar()) != EOF) {
        if (isalpha((unsigned char)ch) || ch == '\'') {
            char c = (char)tolower((unsigned char)ch);
            if (clib_strbuf_append_n(&tok, &c, 1) != CLIB_OK) {
                fprintf(stderr, "out of memory\n");
                return 1;
            }
            continue;
        }
        if (clib_strbuf_len(&tok) == 0) continue;

        if (add_word(&map, clib_strbuf_cstr(&tok)) != 0) {
            fprintf(stderr, "out of memory\n");
            return 1;
        }
        clib_strbuf_clear(&tok);
    }

    /* Flush the final token when stdin does not end on a separator. */
    if (clib_strbuf_len(&tok) && add_word(&map, clib_strbuf_cstr(&tok)) != 0) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    g_used = clib_hashmap_size(&map);
    if (g_used) {
        all = malloc(g_used * sizeof *all);
        if (!all) { fprintf(stderr, "out of memory\n"); return 1; }
        g_out = all;
        g_used = 0;
        clib_hashmap_foreach(&map, collect, NULL);
    }

    if (clib_sort_merge(all, g_used, sizeof all[0], cmp_by_count_desc, NULL) != CLIB_OK) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    {
        size_t i;
        size_t show = ((size_t)limit < g_used) ? (size_t)limit : g_used;
        size_t total = 0;
        for (i = 0; i < g_used; i++) total += all[i].count;
        printf("%zu words, %zu distinct\n\n", total, g_used);
        for (i = 0; i < show; i++)
            printf("%8zu  %s\n", all[i].count, all[i].word);
    }

    free(all);

    /* Keys belong to the map; the counters are ours. Release them first. */
    clib_hashmap_foreach(&map, free_counter, NULL);
    clib_hashmap_free(&map);
    clib_strbuf_free(&tok);
    return 0;
}
