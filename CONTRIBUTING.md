# Contributing to clib

Thanks for wanting to help. This is a learning-oriented library, so **clarity
beats cleverness** in every review.

---

## Before you start

```bash
make test       # must be green before you touch anything
```

---

## The rules

1. **C11 only.** No compiler extensions, no platform-specific headers. If you
   reach for something GNU-ish, it will fail CI.
2. **No warnings.** CI builds with
   `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-qual -Werror`.
   The strict set exists because `-Wcast-qual` already caught a real
   const-correctness hole in `clib_binary_search()`.
3. **Every public function is documented in its header**, including what it does
   on failure and what it does *not* own.
4. **No `abort()`, `exit()` or printing from library code.** Return a
   `clib_status`.
5. **No global state.** If two containers share a hidden buffer, that is a bug.
6. **Nothing recursive** that can be driven to depth by input size. Graph DFS is
   iterative for exactly this reason.

---

## Adding a container

The checklist that every module in this repo followed:

- [ ] `include/clib/<name>.h` — public API, documented, wrapped in
      `extern "C"`, include-guarded.
- [ ] `src/<name>.c` — implementation.
- [ ] Takes a `const clib_allocator *`, accepts `NULL` to mean "the default".
- [ ] Every allocation failure returns `CLIB_ERR_NOMEM` and leaves the container
      **unchanged**.
- [ ] `tests/test_<name>.c` — registered in `tests/clib_test.h` **and**
      `tests/main.c`.
- [ ] Tests cover: empty, one element, many elements, and the OOM path via
      `clib_test_failing_allocator`.
- [ ] A benchmark in `bench/bench.c` if the container has a performance story.
- [ ] Row in the module table in `README.md`.

---

## Writing tests

```c
CLIB_TEST(test_widget)
{
    CLIB_CHECK(cond);                 /* any boolean */
    CLIB_CHECK_EQ(got, want);         /* long long comparison */
    CLIB_CHECK_STR(got, want);        /* string comparison */
}
```

Register the suite in two places — `tests/clib_test.h` (declaration) and
`tests/main.c` (`CLIB_RUN`).

Two allocators exist specifically for the cases ordinary tests miss:

```c
clib_test_fail_after(0);              /* make the very next allocation fail */
...                                    /* assert the container is unchanged  */
clib_test_fail_never();               /* so teardown can still free memory  */
```

`clib_test_counting_allocator` tracks live blocks; assert `live == 0` after a
container is freed to prove there is no leak.

---

## Performance changes need a measurement

Correctness tests cannot catch a performance regression — a degenerate pivot
still sorts correctly, just slowly. That mistake already happened once here and
cost O(n^1.5) on already-sorted input.

If you change an algorithm:

1. Run `make bench` before and after.
2. Include the numbers in the pull request.
3. If a pathological input shape exists, add a **budget** test — count
   comparisons with a wrapper comparator and assert an upper bound, the way
   `test_sort` does. Verify the test fails on the old code before claiming it
   guards anything.

---

## Style

- 4 spaces, no tabs.
- `snake_case` for functions and types, `UPPER_CASE` for macros.
- Public symbols are prefixed `clib_`, and module functions `clib_<module>_`.
- Braces on the same line; always braced, even for one statement.
- Keep functions short enough to read without scrolling.

---

## Pull requests

One logical change per pull request. Describe *why*, not just *what* — the
commit history here reads like a set of engineering notes, and that is
intentional.
