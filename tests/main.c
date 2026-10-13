#include "clib_test.h"
#include <stdlib.h>

int clib_test_checks = 0;
int clib_test_failures = 0;
const char *clib_test_suite = "";

/* --- instrumented allocators -------------------------------------------- */

static void *counting_alloc(void *ptr, size_t size, void *ctx)
{
    clib_test_counter *c = ctx;

    if (size == 0) {
        if (ptr) c->live--;
        free(ptr);
        return NULL;
    }
    if (!ptr) {
        c->live++;
        c->total++;
    }
    return realloc(ptr, size);
}

static void *failing_alloc(void *ptr, size_t size, void *ctx)
{
    clib_test_failure *f = ctx;

    if (size == 0) {
        free(ptr);
        return NULL;
    }
    if (f->calls++ >= f->fail_after) return NULL;
    return realloc(ptr, size);
}

clib_allocator clib_test_counting_allocator = { counting_alloc, NULL };
clib_test_counter clib_test_counter_state = { 0, 0 };

clib_allocator clib_test_failing_allocator = { failing_alloc, NULL };
clib_test_failure clib_test_failure_state = { 0, 0 };

void clib_test_counter_reset(void)
{
    clib_test_counter_state.live = 0;
    clib_test_counter_state.total = 0;
}

void clib_test_fail_after(size_t n)
{
    clib_test_failure_state.fail_after = n;
    clib_test_failure_state.calls = 0;
}

void clib_test_fail_never(void)
{
    clib_test_failure_state.fail_after = (size_t)-1;
    clib_test_failure_state.calls = 0;
}

int main(void)
{
    clib_test_counting_allocator.ctx = &clib_test_counter_state;
    clib_test_failing_allocator.ctx = &clib_test_failure_state;
    clib_test_fail_never();

    CLIB_RUN(test_common);
    CLIB_RUN(test_vector);
    CLIB_RUN(test_slist);
    CLIB_RUN(test_dlist);
    CLIB_RUN(test_stack);
    CLIB_RUN(test_queue);

    printf("\n%d checks, %d failures\n", clib_test_checks, clib_test_failures);
    return clib_test_failures ? 1 : 0;
}
