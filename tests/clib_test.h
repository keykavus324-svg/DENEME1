/**
 * @file clib_test.h
 * @brief Minimal single-header test harness for clib.
 *
 * No dependencies, no dynamic registration: `main()` calls each suite
 * explicitly so the run order is obvious from one file.
 *
 * Also provides two instrumented allocators:
 *  - ::clib_test_counting_allocator - records how many live blocks there are.
 *  - ::clib_test_failing_allocator  - fails on demand, to exercise OOM paths.
 */
#ifndef CLIB_TEST_H
#define CLIB_TEST_H

#include <clib/common.h>
#include <stdio.h>
#include <string.h>

/* --- reporting ---------------------------------------------------------- */

extern int clib_test_checks;
extern int clib_test_failures;
extern const char *clib_test_suite;

/** Declare a suite body: `CLIB_TEST(vector_push) { ... }`. */
#define CLIB_TEST(name) void name(void)

/** Run a suite declared with CLIB_TEST(). */
#define CLIB_RUN(name)                                                    \
    do {                                                                  \
        clib_test_suite = #name;                                          \
        name();                                                           \
    } while (0)

/** Fail the current suite with a message and stop it. */
#define CLIB_FAIL(msg)                                                    \
    do {                                                                  \
        clib_test_failures++;                                             \
        printf("  FAIL %s:%d [%s] %s\n", __FILE__, __LINE__,              \
               clib_test_suite, (msg));                                   \
        return;                                                           \
    } while (0)

#define CLIB_CHECK(cond)                                                  \
    do {                                                                  \
        clib_test_checks++;                                               \
        if (!(cond)) {                                                    \
            clib_test_failures++;                                         \
            printf("  FAIL %s:%d [%s] %s\n", __FILE__, __LINE__,          \
                   clib_test_suite, #cond);                               \
        }                                                                 \
    } while (0)

#define CLIB_CHECK_EQ(got, want)                                          \
    do {                                                                  \
        long long g_ = (long long)(got), w_ = (long long)(want);          \
        clib_test_checks++;                                               \
        if (g_ != w_) {                                                   \
            clib_test_failures++;                                         \
            printf("  FAIL %s:%d [%s] %s == %s (got %lld, want %lld)\n",   \
                   __FILE__, __LINE__, clib_test_suite, #got, #want,       \
                   g_, w_);                                               \
        }                                                                 \
    } while (0)

#define CLIB_CHECK_STR(got, want)                                         \
    do {                                                                  \
        const char *g_ = (got), *w_ = (want);                             \
        clib_test_checks++;                                               \
        if (!g_ || !w_ || strcmp(g_, w_) != 0) {                          \
            clib_test_failures++;                                         \
            printf("  FAIL %s:%d [%s] \"%s\" == \"%s\"\n",                \
                   __FILE__, __LINE__, clib_test_suite,                   \
                   g_ ? g_ : "(null)", w_ ? w_ : "(null)");               \
        }                                                                 \
    } while (0)

/* --- instrumented allocators -------------------------------------------- */

/** Counts live blocks; a non-zero `live` after a test means a leak. */
typedef struct clib_test_counter {
    size_t live;        /**< Blocks currently allocated. */
    size_t total;       /**< Blocks ever allocated. */
} clib_test_counter;

/** Allocator that updates a ::clib_test_counter. */
extern clib_allocator clib_test_counting_allocator;
/** Backing counter for ::clib_test_counting_allocator. */
extern clib_test_counter clib_test_counter_state;

/** Reset the counting allocator. */
void clib_test_counter_reset(void);

/**
 * Allocator that returns NULL once `fail_after` successful calls have gone by.
 * Set `fail_after` to 0 to make the very next allocation fail.
 */
typedef struct clib_test_failure {
    size_t fail_after;  /**< Allow this many allocations, then fail. */
    size_t calls;       /**< Allocations seen so far. */
} clib_test_failure;

extern clib_allocator clib_test_failing_allocator;
extern clib_test_failure clib_test_failure_state;

/** Arm the failing allocator: fail on the `n`-th next allocation (0-based). */
void clib_test_fail_after(size_t n);

/** Disarm the failing allocator so teardown can still free memory. */
void clib_test_fail_never(void);

/* --- suites -------------------------------------------------------------- */

void test_common(void);
void test_vector(void);
void test_slist(void);
void test_dlist(void);
void test_stack(void);
void test_queue(void);
void test_hashmap(void);
void test_bst(void);
void test_sort(void);
void test_heap(void);
void test_strutil(void);
void test_graph(void);
void test_lru(void);

#endif /* CLIB_TEST_H */
