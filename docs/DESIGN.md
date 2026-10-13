# clib — Design notes

This file records *why* things are the way they are. The headers say what each
function does; this says what it cost to decide.

---

## 1. Ownership: the three rules

Every bug I have ever shipped in C was, eventually, an ownership bug. clib makes
the rule explicit per container:

| Container | Keys | Values |
| --------- | ---- | ------ |
| `clib_vector(T)` | n/a | **copied** (it stores `T` by value) |
| `clib_slist`, `clib_dlist` | n/a | **borrowed** – `clear()` frees the nodes, never the values |
| `clib_stack`, `clib_queue` | n/a | **borrowed** |
| `clib_hashmap` | **copied** (`strdup`-style) | **borrowed** |
| `clib_bst` | **borrowed** | **borrowed** |
| `clib_heap` | n/a | **borrowed** |
| `clib_graph` | n/a | n/a – vertices are dense indices |

`clib_strbuf` is the only container that owns a byte buffer rather than a
pointer.

Everything that frees memory takes the allocator it was created with, so a
container built on an arena can be dropped without the library ever touching
`free()`.

---

## 2. Generics without code bloat: `clib_vector(T)`

C has no templates. The three usual answers all have a cost:

1. `void *` everywhere – no type safety, and every access is a cast.
2. Macro-generated *functions* – type safe, but the growth logic is compiled
   once per type.
3. Macro-generated *structs* over one generic core – type safe, one copy of the
   logic.

`clib_vector(T)` takes option 3. The macro declares

```c
struct { T *data; size_t size; size_t capacity; const clib_allocator *alloc;
         size_t _elem_size; }
```

which is layout-identical to `clib_vector_raw`, so a typed vector can be handed
to the shared implementation with a plain cast.

The obvious danger is that someone later adds a field to one struct and not the
other, at which point the cast starts lying. `src/vector.c` prevents that at
compile time:

```c
_Static_assert(sizeof(clib_vector_probe) == sizeof(clib_vector_raw),
               "clib_vector(T) and clib_vector_raw layouts diverged");
_Static_assert(_Alignof(clib_vector_probe) == _Alignof(clib_vector_raw), ...);
```

Silent memory corruption became a build error. That trade — one odd-looking
`_elem_size` field in exchange for a compile-time guarantee — is worth it.

---

## 3. The allocator is not optional-looking

`clib_allocator` is a single `realloc`-shaped callback:

```c
void *(*alloc_fn)(void *ptr, size_t size, void *ctx);
```

with the contract that `size == 0` frees and returns NULL. One function covers
malloc, realloc and free, so a custom allocator only has to implement one thing
and cannot get `free` out of sync with `alloc`.

Passing `NULL` anywhere an allocator is expected selects the process default,
which keeps the simple call sites short.

Tests use this to do something that is otherwise very hard to check: a
*failing* allocator that returns NULL on demand. Every container has an
assertion that a failed allocation leaves it **unchanged**, not half-built.

---

## 4. Hash map: the tombstone trap

Open addressing with linear probing cannot simply blank a slot on delete,
because the blank would stop probes from finding entries that collided past it.
So deletions leave a *tombstone*.

Two things then go wrong in most first implementations:

**The load factor lies.** If growth is triggered by `size / capacity`, a map
that only ever inserts and deletes fills up with tombstones that count as
neither, and lookups degrade to O(n) while the map believes it is 60% full.
Here the trigger is

```c
(m->size + m->tombstones + 1) / m->capacity > 0.70
```

**Tombstones are never reclaimed.** Even with a correct load factor, a
delete-heavy map can carry more tombstones than live entries, wasting probes on
every single lookup. So `clib_hashmap_remove()` rebuilds the table *at the same
capacity* once `tombstones > size`, which clears them at O(n) instead of letting
them accumulate. The test suite asserts `tombstones <= size` after deleting 400
of 600 entries.

Keys are copied; values are borrowed.

---

## 5. Quicksort: the bug the benchmarks caught

This one is worth writing down because the code was **correct** the whole time.
It was just quadratic-ish, and no correctness test could see that.

3-way (Dutch national flag) partitioning was chosen so that arrays full of
duplicates cost O(n) instead of O(n²) — that part worked. Pivots came from
median-of-three. Then the first benchmark run produced:

```
quicksort / random          0.043 s
quicksort / already sorted  0.434 s     <-- 10x slower
```

Sorting an already-sorted array should be the *fast* case. Measuring the
scaling showed ~2.8× per doubling of n, i.e. **O(n^1.5)**, not O(n log n).
At n = 160,000 that was 34.1M comparisons where ~2.1M were expected.

Two mechanisms combined:

1. The `> pivot` branch of a 3-way partition swaps elements to the end, which
   **rotates** that region. Partitioning sorted data therefore leaves the right
   half as `[34, 35, …, 63, 33]` — sorted, except the minimum sits last.
2. Median-of-three then samples `(first, middle, last)` = `(34, 49, 33)` and
   picks **34** as the median: almost the minimum of the range. The split was
   1-vs-29, and the same thing happened again one level down.

The fix is Tukey's ninther — the median of three medians-of-three over nine
spread-out samples. One rotation cannot fool nine samples:

```
n = 160,000, already sorted
  before:  0.3155 s   34.1M comparisons   (213 per element)
  after:   0.0173 s    2.6M comparisons   ( 16 per element)   18x faster
```

Sampling is also done *by value* now — the classic Bentley–McIlroy version
physically sorts its three samples into place, which is extra writes for no
benefit when you only need the value.

**The lesson, generalised:** a correctness test cannot catch this, because a
degenerate pivot still sorts correctly. The regression guard counts comparisons
through a wrapper comparator and asserts a budget of 64 per element (actual
usage ~16, the broken version ~200). I verified the guard by reverting the fix:
it fails on 4 of the 6 adversarial patterns.

Below 40 elements the nine samples cost more than they are worth, so
median-of-three is used there.

---

## 6. Why the traversals are not recursive

`clib_graph_dfs()` could be eight lines of recursion. It is not, because a
10-million-vertex chain graph is a stack overflow waiting to happen, and
"the library crashed on your input" is not an acceptable failure mode.

Instead it keeps an explicit `clib_stack` of *pointers into an index array*, and
recovers the vertex number with pointer subtraction:

```c
size_t u = (size_t)((size_t *)clib_stack_pop(&st) - ids);
```

The alternative — `(void *)(uintptr_t)i` — is implementation-defined. Pointer
arithmetic on a real array is not. It costs one small allocation and removes a
cast that would be quietly undefined elsewhere.

Dijkstra uses the same trick with `clib_heap`, plus *lazy deletion*: stale
entries are left in the heap and skipped via a `done[]` flag on pop. That avoids
needing a decrease-key operation, which would require an external index handle
into the heap.

---

## 7. Error handling: return codes, no hidden aborts

No function in clib calls `abort()`, `exit()`, or prints on failure. Everything
that can fail returns a `clib_status`, and `clib_status_string()` turns it into
text for the caller to render however it likes.

The invariant the tests enforce is *strong*: if an operation fails, the
container is byte-for-byte as it was before. A failed `clib_vector_push()` does
not leave `size` incremented; a failed `clib_hashmap_put()` does not leave a
half-linked entry.

---

## 8. What this library deliberately does not have

- **A self-balancing tree.** `clib_bst` is unbalanced on purpose; sorted inserts
  degrade it to a list and the header says so in as many words. An AVL or red-black
  tree is a genuinely different data structure with its own invariants, and
  bolting one on would double the code for a container nobody should pick.
- **`decrease-key` on the heap.** It needs an external index handle; callers who
  need it almost always want a different structure.
- **Thread safety.** There is no hidden global state, so separate containers can
  be used from separate threads freely — but a single container is not
  synchronised, and pretending otherwise with internal locks would cost every
  single-threaded caller.
- **Iterator invalidation rules written in prose.** Each header states them
  directly instead.

---

## 9. Testing strategy

Three layers, in order of how much they actually catch:

1. **Instrumented allocators.** A counting allocator (leak detection) and a
   failing allocator (OOM paths). These found real bugs; "it works" tests did not.
2. **Adversarial input shapes.** Sorted, reverse-sorted, all-equal, organ-pipe,
   two-distinct-values, and every size from 0 to 70 to straddle the
   insertion-sort cutoff. Small-array edge cases fail exactly at boundaries.
3. **Budgets, not just assertions.** Comparison counts for the sort, tombstone
   counts for the hash map, power-of-two invariants for ring buffer and table
   capacities. These are the only tests that can see a performance regression.

CI adds sanitizers (ASan catches leaks by default on Linux, UBSan with
`-fno-sanitize-recover=all`), a `-Werror` job with `-Wconversion
-Wsign-conversion -Wcast-qual -Wshadow`, and a job that compiles every header
standalone as both C and C++ — a header that only works when something else is
included first is a bug.
