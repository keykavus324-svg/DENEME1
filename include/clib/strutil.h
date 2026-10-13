/**
 * @file strutil.h
 * @brief Growable string buffer plus small read-only string helpers.
 *
 * ::clib_strbuf owns its memory and always keeps a NUL terminator, so
 * `clib_strbuf_cstr()` is safe to hand to any C API. It never stores embedded
 * NULs, which keeps the invariants simple.
 */
#ifndef CLIB_STRUTIL_H
#define CLIB_STRUTIL_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A growable, NUL-terminated byte buffer. */
typedef struct clib_strbuf {
    char *data;                 /**< NUL-terminated, never NULL after init. */
    size_t len;                 /**< Bytes excluding the terminator. */
    size_t capacity;            /**< Bytes allocated, including terminator. */
    const clib_allocator *alloc;
} clib_strbuf;

/** Initialise to an empty string. Returns CLIB_ERR_NOMEM if the first alloc fails. */
clib_status clib_strbuf_init(clib_strbuf *b, const clib_allocator *a);

/** Append NUL-terminated @p s. */
clib_status clib_strbuf_append(clib_strbuf *b, const char *s);

/** Append @p n bytes, which may include NULs. */
clib_status clib_strbuf_append_n(clib_strbuf *b, const char *s, size_t n);

/** printf-style append. */
clib_status clib_strbuf_appendf(clib_strbuf *b, const char *fmt, ...);

/** Read-only view; never NULL once the buffer is initialised. */
const char *clib_strbuf_cstr(const clib_strbuf *b);

/** Current length, excluding the terminator. */
size_t clib_strbuf_len(const clib_strbuf *b);

/** Truncate to zero bytes, keeping the allocation. */
void clib_strbuf_clear(clib_strbuf *b);

/** Free the buffer and reset the struct. */
void clib_strbuf_free(clib_strbuf *b);

/* --- read-only helpers on plain C strings ------------------------------- */

/** 1 when @p s starts with @p prefix. */
int clib_str_starts_with(const char *s, const char *prefix);

/** 1 when @p s ends with @p suffix. */
int clib_str_ends_with(const char *s, const char *suffix);

/** Case-insensitive equality for ASCII. */
int clib_str_ieq(const char *a, const char *b);

/** Trim leading and trailing ASCII whitespace, in place. Returns @p s. */
char *clib_str_trim(char *s);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_STRUTIL_H */
