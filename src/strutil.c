#include <clib/strutil.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CLIB_STRBUF_MIN_CAP 32u

clib_status clib_strbuf_init(clib_strbuf *b, const clib_allocator *a)
{
    if (!b) return CLIB_ERR_INVAL;
    b->data = clib_alloc(a, CLIB_STRBUF_MIN_CAP);
    if (!b->data) { b->len = b->capacity = 0; b->alloc = a; return CLIB_ERR_NOMEM; }
    b->data[0] = '\0';
    b->len = 0;
    b->capacity = CLIB_STRBUF_MIN_CAP;
    b->alloc = a;
    return CLIB_OK;
}

/** Ensure room for @p extra bytes on top of the current length plus terminator. */
static clib_status reserve(clib_strbuf *b, size_t extra)
{
    size_t need, cap;
    char *p;

    if (extra > (size_t)-1 - b->len - 1) return CLIB_ERR_NOMEM;
    need = b->len + extra + 1;
    if (need <= b->capacity) return CLIB_OK;

    cap = b->capacity ? b->capacity : CLIB_STRBUF_MIN_CAP;
    while (cap < need) {
        if (cap > ((size_t)-1) / 2) { cap = need; break; }
        cap *= 2;
    }

    p = clib_realloc(b->alloc, b->data, cap);
    if (!p) return CLIB_ERR_NOMEM;

    b->data = p;
    b->capacity = cap;
    return CLIB_OK;
}

clib_status clib_strbuf_append(clib_strbuf *b, const char *s)
{
    return clib_strbuf_append_n(b, s, s ? strlen(s) : 0);
}

clib_status clib_strbuf_append_n(clib_strbuf *b, const char *s, size_t n)
{
    clib_status st;
    if (!b || !s) return CLIB_ERR_INVAL;
    if (n == 0) return CLIB_OK;

    st = reserve(b, n);
    if (st != CLIB_OK) return st;

    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return CLIB_OK;
}

clib_status clib_strbuf_appendf(clib_strbuf *b, const char *fmt, ...)
{
    va_list ap;
    int n;
    clib_status st;

    if (!b || !fmt) return CLIB_ERR_INVAL;

    /* Measure first, then reserve: keeps formatting out of the growth path. */
    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return CLIB_ERR_INVAL;

    st = reserve(b, (size_t)n);
    if (st != CLIB_OK) return st;

    va_start(ap, fmt);
    vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap);
    va_end(ap);

    b->len += (size_t)n;
    b->data[b->len] = '\0';
    return CLIB_OK;
}

const char *clib_strbuf_cstr(const clib_strbuf *b)
{
    return (b && b->data) ? b->data : "";
}

size_t clib_strbuf_len(const clib_strbuf *b)
{
    return b ? b->len : 0;
}

void clib_strbuf_clear(clib_strbuf *b)
{
    if (!b || !b->data) return;
    b->len = 0;
    b->data[0] = '\0';
}

void clib_strbuf_free(clib_strbuf *b)
{
    if (!b) return;
    clib_free(b->alloc, b->data);
    b->data = NULL;
    b->len = b->capacity = 0;
}

int clib_str_starts_with(const char *s, const char *prefix)
{
    size_t n;
    if (!s || !prefix) return 0;
    n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

int clib_str_ends_with(const char *s, const char *suffix)
{
    size_t ls, lsuf;
    if (!s || !suffix) return 0;
    ls = strlen(s);
    lsuf = strlen(suffix);
    if (lsuf > ls) return 0;
    return strncmp(s + ls - lsuf, suffix, lsuf) == 0;
}

int clib_str_ieq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        /* Cast via unsigned char: tolower() is UB on negative values. */
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) return 0;
    }
    return *a == *b;
}

char *clib_str_trim(char *s)
{
    char *end;
    size_t len;

    if (!s || !*s) return s;

    while (*s && isspace((unsigned char)*s)) s++;

    len = strlen(s);
    if (len == 0) return s;

    end = s + len - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}
