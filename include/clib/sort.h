/**
 * @file sort.h
 * @brief Generic sorting and searching over contiguous arrays.
 *
 * Every routine works on raw bytes plus an element size and a ::clib_cmp_fn,
 * which keeps one implementation reusable for any type. The comparison
 * convention is the usual one: negative when `lhs < rhs`, 0 when equal,
 * positive when `lhs > rhs`.
 *
 * | Routine | Average | Worst | Stable | Extra memory |
 * | ------- | ------- | ----- | ------ | ------------ |
 * | clib_sort_insertion | O(n^2) | O(n^2) | yes | O(1) |
 * | clib_sort_quick     | O(n log n) | O(n log n) | no | O(log n) stack + O(es) |
 * | clib_sort_merge     | O(n log n) | O(n log n) | **yes** | O(n*es) |
 */
#ifndef CLIB_SORT_H
#define CLIB_SORT_H

#include <clib/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Below this many elements, quicksort switches to insertion sort. */
#define CLIB_SORT_INSERTION_CUTOFF 16u

/** Insertion sort. O(n^2), but the fastest option for tiny arrays. */
void clib_sort_insertion(void *base, size_t n, size_t elem_size,
                         clib_cmp_fn cmp, void *ctx);

/**
 * Quicksort with median-of-three pivoting and a 3-way partition.
 *
 * The 3-way (Dutch national flag) partition is what keeps arrays full of
 * duplicates at O(n) instead of degenerating to O(n^2), and the median-of-three
 * pivot avoids the already-sorted worst case. Recursion is bounded at O(log n)
 * stack depth because only the smaller side recurses.
 */
void clib_sort_quick(void *base, size_t n, size_t elem_size,
                     clib_cmp_fn cmp, void *ctx);

/**
 * Bottom-up merge sort. Stable, O(n log n) worst case, allocates one scratch
 * buffer of `n * elem_size`. Returns CLIB_ERR_NOMEM if that fails.
 */
clib_status clib_sort_merge(void *base, size_t n, size_t elem_size,
                            clib_cmp_fn cmp, void *ctx);

/**
 * Binary search over a sorted array. Returns a pointer to a matching element,
 * or NULL when @p key is absent (then which element matches is unspecified).
 */
void *clib_binary_search(const void *key, const void *base, size_t n,
                         size_t elem_size, clib_cmp_fn cmp, void *ctx);

/** True when the array is already sorted in ascending order. */
int clib_is_sorted(const void *base, size_t n, size_t elem_size,
                   clib_cmp_fn cmp, void *ctx);

/** Reverse the elements in place. */
void clib_reverse(void *base, size_t n, size_t elem_size);

#ifdef __cplusplus
}
#endif

#endif /* CLIB_SORT_H */
