# clib

[![CI](https://github.com/keykavus324-svg/DENEME1/actions/workflows/ci.yml/badge.svg)](https://github.com/keykavus324-svg/DENEME1/actions/workflows/ci.yml)
![C](https://img.shields.io/badge/C-C11-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

A small, dependency-free **C11 utility library**: generic containers, sorting
routines, string helpers and graph algorithms — written to be readable first.

Every module is self-contained, has unit tests and is documented in the header.

> **Türkçe:** Bu repo, C99 ile yazılmış, bağımlılığı olmayan küçük bir yardımcı
> kütüphanedir. Her modülün birim testi vardır ve başlık dosyasında belgelenmiştir.

---

## Why?

Learning data structures is easier when you can *read* the whole implementation.
`clib` keeps every container under a few hundred lines, with no macros that hide
control flow and no hidden global state.

## Modules

| Module | Header | Status |
| ------ | ------ | ------ |
| Dynamic array | `clib/vector.h` | ✅ |
| Singly linked list | `clib/slist.h` | ✅ |
| Doubly linked list | `clib/dlist.h` | ✅ |
| Stack | `clib/stack.h` | ✅ |
| Queue (ring buffer) | `clib/queue.h` | ✅ |
| Hash map | `clib/hashmap.h` | ✅ |
| Binary search tree | `clib/bst.h` | ✅ |
| Binary min-heap | `clib/heap.h` | ✅ |
| Sorting | `clib/sort.h` | ✅ |
| String helpers | `clib/strutil.h` | ✅ |
| Graph (BFS/DFS/Dijkstra) | `clib/graph.h` | ✅ |

## Quick start

```bash
git clone https://github.com/keykavus324-svg/DENEME1.git clib
cd clib
make            # builds build/libclib.a
make test       # builds and runs the unit-test suite
make bench      # builds and runs the benchmarks
```

```c
#include <clib/vector.h>
#include <stdio.h>

int main(void) {
    clib_vector(int) v;      /* vector of int */
    clib_vector_init(&v);

    for (int i = 0; i < 10; i++)
        clib_vector_push(&v, i * i);

    for (size_t i = 0; i < clib_vector_size(&v); i++)
        printf("%d ", v.data[i]);

    clib_vector_free(&v);
    return 0;
}
```

## Design rules

1. **Explicit allocation.** Every container takes an allocator in its `*_init`
   call; default is `malloc/free`.
2. **Return codes, not hidden aborts.** Functions that can fail return a
   `clib_status`.
3. **No global state.** Two containers never share hidden buffers.
4. **C11 only.** No compiler extensions, no platform-specific headers.

## License

MIT — see [LICENSE](LICENSE).
