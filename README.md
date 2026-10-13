# clib

[![CI](https://github.com/keykavus324-svg/DENEME1/actions/workflows/ci.yml/badge.svg)](https://github.com/keykavus324-svg/DENEME1/actions/workflows/ci.yml)
[![C11](https://img.shields.io/badge/C-C11-blue.svg)](https://en.cppreference.com/w/c/language/history)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-17%2C340%20checks-brightgreen.svg)](#testing)

A small, dependency-free **C11 utility library**: containers, sorting, string
helpers and graph algorithms — written to be *read* first, used second.

No macros that hide control flow. No global state. No hidden `abort()`.

> **Türkçe:** Bağımlılığı olmayan, C11 ile yazılmış küçük bir yardımcı kütüphane.
> Her modül kendi kendine yeterli, birim testli ve başlık dosyasında belgelenmiştir.
> Tasarım kararlarının gerekçesi [docs/DESIGN.md](docs/DESIGN.md) dosyasında.

---

## Why?

Learning data structures is easier when you can read the whole implementation.
`clib` keeps every container under a few hundred lines and documents the *why*
of each decision — including the bugs that were found and fixed along the way.

---

## Quick start

```bash
git clone https://github.com/keykavus324-svg/DENEME1.git clib
cd clib
make            # builds build/libclib.a
make test       # builds and runs the unit tests
make bench      # builds and runs the micro-benchmarks
make examples   # builds the example programs
```

```c
#include <clib/vector.h>
#include <stdio.h>

int main(void) {
    clib_vector(int) v;
    clib_vector_init(&v);

    for (int i = 0; i < 10; i++)
        (void)clib_vector_push(&v, i * i);

    for (size_t i = 0; i < clib_vector_size(&v); i++)
        printf("%d ", v.data[i]);

    clib_vector_free(&v);
    return 0;
}
```

---

## Modules

| Module | Header | Notes |
| ------ | ------ | ----- |
| Dynamic array | `clib/vector.h` | type-safe macros over one generic core |
| Singly linked list | `clib/slist.h` | O(1) push_front/push_back |
| Doubly linked list | `clib/dlist.h` | O(1) erase, `move_to_front` for LRU |
| Stack | `clib/stack.h` | contiguous buffer, LIFO |
| Queue | `clib/queue.h` | ring buffer, power-of-two capacity |
| Hash map | `clib/hashmap.h` | open addressing, FNV-1a, tombstoned |
| Binary search tree | `clib/bst.h` | unbalanced by design |
| Binary min-heap | `clib/heap.h` | flat array, no recursion |
| Sorting & searching | `clib/sort.h` | quick / merge / insertion + binary search |
| String helpers | `clib/strutil.h` | growable `clib_strbuf` + read-only helpers |
| Graph | `clib/graph.h` | BFS, DFS, Dijkstra |
| LRU cache | `clib/lru.h` | hash map + list, O(1) get/put/evict |
| Disjoint set | `clib/uf.h` | union by rank + path compression |
| Prefix tree | `clib/trie.h` | O(len) lookup, prefix enumeration |
| Arena allocator | `clib/arena.h` | bump allocation, plugs into containers |

---

## Examples

Two programs that use the library for something real, both built by
`make examples` and run under AddressSanitizer in CI:

```bash
# Word frequencies: hash map + string buffer + stable merge sort
printf "the cat the hat sat on the mat and the cat sat" | ./build/wordcount 5

# Shortest path across a weighted grid: graph + heap + doubly linked list
./build/shortest_path 12 16 7
```

```
shortest path: 0 -> 191, cost 80, 27 steps

#...............
####............
...#............
...#............
...#............
...####.........
......###.......
```

---

## Design rules

1. **Explicit ownership.** Every container states in its header whether it owns
   or borrows keys and values. See the table in [docs/DESIGN.md](docs/DESIGN.md).
2. **Return codes, not hidden aborts.** Nothing in the library calls `abort()`,
   `exit()` or prints on failure.
3. **Strong failure guarantee.** If an operation fails, the container is left
   exactly as it was — verified with an allocator that fails on demand.
4. **No global state.** Containers never share hidden buffers.
5. **C11 only.** No compiler extensions, no platform-specific headers.

---

## Testing

`make test` runs **17,340 assertions**. Three layers, in order of how much they
actually catch:

- **Instrumented allocators** — one that counts live blocks (leak detection) and
  one that fails on demand (OOM paths). These found real bugs; "it works" tests
  did not.
- **Adversarial input shapes** — sorted, reverse-sorted, all-equal, organ-pipe,
  two-distinct-values, and every size from 0 to 70 to straddle the
  insertion-sort cutoff.
- **Budgets** — comparison counts for sorting, tombstone counts for the hash
  map, power-of-two invariants for capacities. These are the only tests that can
  see a *performance* regression.

CI adds gcc/clang × debug/release, ASan + UBSan, a `-Werror` build with
`-Wconversion -Wsign-conversion -Wcast-qual -Wshadow`, benchmarks, and a job
that compiles every header standalone as both C and C++.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). In short: `make test` must pass, new
code needs tests, and the strict warning set has to stay clean.

---

## License

MIT — see [LICENSE](LICENSE).
