# LRU Cache & Memory Management Engine

A compact C++17 project that demonstrates cache eviction, pointer-safe list management, explicit memory ownership, and constrained-memory design. It implements a reusable, byte-budgeted Least Recently Used (LRU) cache with a custom doubly linked list, `std::unordered_map`, and a cache-local node pool.

> This is a software cache and CLI simulation. It does not implement a hardware cache, operating-system subsystem, device driver, or embedded target integration.

## Features

- Average **O(1)** lookup, insertion/update, and erase using a hash map plus a doubly linked recency list.
- LRU eviction from a configurable logical byte budget, with an optional maximum entry count.
- Lazy node pool that reuses node storage after eviction or erase and releases all storage at cache destruction.
- RAII ownership: the cache owns its nodes and pool; callers receive copied output or a short-lived pointer from `get`.
- Linux-friendly interactive CLI, deterministic workload benchmark, self-contained unit tests, CMake build, and GitHub Actions CI.
- Hit, miss, eviction, logical-byte, and pool-slot counters for inspection.

## Architecture

```text
                         LruCache<Key, Value>
             ┌──────────────────────────────────┐
             │ get / put / erase / clear / stats│
             └─────────────┬────────────────────┘
                           │
         ┌─────────────────┴──────────────────┐
         │                                    │
 ┌───────▼─────────┐                  ┌───────▼──────────┐
 │ unordered_map   │                  │ Doubly linked    │
 │ key -> Node*    │                  │ MRU <-> ... <->LRU│
 └─────────────────┘                  └──────────────────┘
                           │
                   ┌───────▼────────┐
                   │ MemoryPool<Node>│
                   │ reuses free slots│
                   └────────────────┘
```

The list has a circular sentinel, so moving a node or removing the least-recently-used node requires a constant number of pointer updates. A successful `get` and every `put` move the entry to the MRU end. When a new or larger value would exceed the budget, entries are evicted from the LRU end until it fits. Values larger than the full budget are rejected without changing the cache.

## Memory budget and ownership

`used_bytes()` is a **logical estimate**, not a process RSS measurement. It charges `sizeof(Node)` for each entry and adds the lengths of `std::string` keys and values. For other key/value types, only `sizeof(Node)` is charged. Hash-table buckets, allocator headers, string capacity beyond length, and implementation-specific metadata are not included. The optional entry limit provides a separate hard bound on the number of cached nodes.

`MemoryPool<Node>` allocates slots on demand, retains freed slots for reuse, and owns the raw storage until destruction. Each live node is explicitly constructed and destroyed in a slot. Strings and other members still use their normal C++ allocators. Consequently, the pool reduces repeated node-storage allocations after warm-up; it does not claim a fully fixed-size arena or a strict physical-memory ceiling.

The cache is not internally synchronized. Use external synchronization if multiple threads access one instance. The pointer returned by `get(key)` remains valid only until the next non-const cache operation that can evict or erase that entry, or until the cache is destroyed. Prefer `get(key, out)` when a stable copy is needed.

## Complexity

| Operation | Average time | Additional space |
|---|---:|---:|
| `get` | O(1) | O(1) |
| `put` / update | O(1) average, plus O(k) evictions | O(1) per new entry |
| `erase` | O(1) average | O(1) |
| `clear` | O(n) | O(1) |

Hash table operations are average O(1), with O(n) worst-case behavior under pathological collisions. Inserting one item can evict `k` entries, so that individual call takes O(k); across a sequence, each evicted item is removed once.

## Build

Requirements: CMake 3.16+ and a C++17 compiler (GCC, Clang, or MSVC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

The tests cover recency updates, eviction order, updates, erase, clear, byte-budget rejection, and node-slot reuse. No external test framework or package download is needed.

## CLI

```bash
./build/lru-cli --budget 4096
```

On Windows, run `build\lru-cli.exe --budget 4096` from PowerShell. Commands use whitespace-separated keys and values:

```text
PUT page-1 payloadA
PUT page-2 payloadB
GET page-1
ORDER
STATS
ERASE page-2
CLEAR
QUIT
```

`ORDER` prints entries from MRU to LRU. The CLI accepts a single token for each key and value to keep the command parser intentionally small.

## Benchmark

```bash
./build/lru-benchmark                 # defaults: 1,000,000 operations, 16,384-key working set
./build/lru-benchmark 500000 8192     # operations, working-set size
```

The executable generates a deterministic pseudo-random access trace and prints elapsed time, measured operations per second, hits, misses, evictions, and a checksum. Results depend on the compiler, build type, CPU, operating system, and machine load; no benchmark figures are claimed in advance. Build in Release mode and compare repeated runs on the same machine for useful measurements.

## Project layout

```text
include/lru_cache/   Reusable cache and memory-pool headers
src/                Interactive CLI
tests/              Self-contained unit tests
benchmarks/         Deterministic benchmark driver
.github/workflows/  Build and test CI
```

## Design choices and extensions

- A map plus list is chosen to make both key lookup and recency updates average O(1).
- Nodes are linked with non-owning raw pointers for predictable constant-time splicing; the pool owns the underlying storage and the cache controls each node's lifetime.
- The logical budget makes constrained-memory behavior observable while documenting exactly which bytes it estimates.
- The current implementation is single-threaded. A next step could add a mutex and measure contention, or add a value-size policy for arbitrary types.
- This is suitable for discussing memory ownership and cache behavior in an interview; it should not be presented as a production embedded allocator or as an actual device-driver implementation.

## Interview talking points

1. Why does a singly linked list make LRU eviction less convenient than a doubly linked list?
2. How do the map and list invariants stay synchronized during insertion, update, and eviction?
3. What is the difference between average and worst-case hash-map complexity?
4. Why is the configured byte budget an estimate, and what would be required for strict accounting?
5. What are the exception-safety and thread-safety trade-offs of the chosen ownership model?
6. How could a fixed-capacity slab or allocator-aware container change fragmentation and memory predictability?

## License

No license has been selected. Add one only after choosing the terms under which you want others to use this project.
