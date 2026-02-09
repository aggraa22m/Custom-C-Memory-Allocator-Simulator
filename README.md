# Lock-Free Queue with Comprehensive Testing (C11)

This project implements a **Michael–Scott lock-free queue** using **C11 atomics**, alongside a traditional **mutex-based queue** for comparison. The implementation is validated with extensive **correctness tests, concurrent stress tests, and performance benchmarks**.

The goal of this project is not just to implement a data structure, but to demonstrate **correct concurrent reasoning, memory-order awareness, and disciplined testing** — skills required in systems programming, embedded systems, and high-performance software.

---

## Project Objectives

* Implement a well-known **lock-free queue algorithm** using CAS loops
* Contrast lock-free vs lock-based synchronization under contention
* Explore **memory reclamation challenges** in concurrent data structures
* Validate correctness through deterministic and concurrent tests
* Measure performance scaling across increasing thread counts

---

## Why This Project Matters

Concurrent programming is hard because:

* threads interleave unpredictably
* bugs may only appear under high contention
* memory reclamation is non-trivial without locks

This project demonstrates:

* practical use of **C11 atomic operations**
* lock-free algorithm implementation from academic literature
* awareness of real-world pitfalls (ABA, reclamation, linearizability)
* testing beyond simple single-threaded cases

---

## Algorithm Overview

The lock-free queue is based on the **Michael & Scott (1996)** algorithm:

* A dummy node is always present
* Enqueue and dequeue operations use **compare-and-swap (CAS)** loops
* `head` and `tail` pointers advance independently
* Threads help advance the tail when lagging

This design ensures:

* no global locks
* progress for at least one thread (lock-free property)

---

## Memory Reclamation Strategy

Immediate `free()` is unsafe in lock-free structures because other threads may still access nodes.

This implementation uses **deferred reclamation**:

* dequeued nodes are placed into a retired list
* nodes are freed later in a controlled cleanup phase

⚠️ **Note**: This is a simplified approach intended for instructional purposes. Production systems typically use:

* hazard pointers
* epoch-based reclamation
* reference counting

---

## Project Structure

```
project/
├── Project3.c        # Full implementation, tests, and benchmarks
├── README.md
```

(The project was originally developed as a single-file academic submission.)

---

## Build Instructions

### Requirements

* GCC or Clang with C11 support
* POSIX threads
* Linux or macOS

### Compile

```bash
gcc -std=c11 -O2 -pthread Project3.c -o lockfree_queue
```

### Run

```bash
./lockfree_queue
```

---

## Test Results

### Summary

**All 210 test cases passed successfully with 100% pass rate.**

The implementation has been validated through comprehensive testing covering:

* Correctness under single-threaded and multi-threaded scenarios
* Edge cases and boundary conditions
* Performance characteristics across 1-32 threads
* FIFO ordering preservation
* Thread safety under high contention

### Detailed Test Breakdown

#### Total Test Cases: 210

* **Core correctness tests: 10**
* **Bonus test cases: 200**
  * FIFO varying sizes: 100 tests
  * Empty dequeue: 10 tests
  * Single item: 10 tests
  * Alternating operations: 20 tests
  * Boundary values: 10 tests
  * Rapid operations: 10 tests
  * Bulk operations: 15 tests
  * Interleaved patterns: 10 tests
  * Negative values: 5 tests
  * Sequential patterns: 10 tests

---

## Test Coverage

### Core Correctness Tests (10)

* empty queue behavior
* single enqueue/dequeue
* FIFO order preservation
* bulk enqueue/dequeue
* alternating operations
* concurrent producers
* concurrent consumers
* mixed producer/consumer workloads
* stress testing with large data sets
* lock-based queue correctness

### Bonus Tests (190+)

Additional test sets validate:

* varying queue sizes
* boundary values (negative and large integers)
* rapid operation sequences
* interleaved enqueue/dequeue patterns
* sequential access patterns

These tests are designed to expose race conditions and ordering errors.

---

## Performance Benchmarking

The project includes benchmarks comparing:

* lock-based queue (mutex-protected)
* lock-free queue (CAS-based)

Benchmarks are executed across:

* 1, 2, 4, 8, 16, and 32 threads
* mixed enqueue/dequeue workloads
* 50,000 operations per thread (50% enqueue, 50% dequeue)

### Performance Results

| Thread Count | Lock-Based Time (s) | Lock-Free Time (s) | Speedup   |
|--------------|---------------------|--------------------|-----------|
| 1            | 0.0026              | 0.0039             | 0.66x     |
| 2            | 0.0156              | 0.0117             | 1.34x     |
| 4            | 0.0495              | 0.0231             | **2.15x** |
| 8            | 0.0843              | 0.0463             | 1.82x     |
| 16           | 0.1814              | 0.0938             | 1.93x     |
| 32           | 0.4046              | 0.1931             | 2.10x     |

### Key Findings

* **Single-threaded overhead**: Lock-free implementation is 0.66x slower at 1 thread due to atomic operation overhead
* **Crossover point**: Lock-free queue becomes faster at 2+ threads (1.34x speedup)
* **Peak efficiency**: Maximum relative performance of 2.15x achieved at 4 threads
* **High contention**: Maintains ~2.1x speedup at 32 threads, demonstrating excellent scalability
* **Lock-based degradation**: Near-linear performance degradation as thread count increases
* **Lock-free stability**: Relatively stable performance across increasing thread counts

The lock-free queue demonstrates significant advantages under high thread contention, with approximately **2.1x performance improvement at 32 threads** compared to the mutex-based baseline.

### Performance Visualization

For detailed performance graphs and analysis, refer to the [Project Report](Project3_AshishAggrawal.pdf), which includes:

* **Execution Time Comparison Graph** (Page 10): Bar chart comparing lock-based vs lock-free execution times across thread counts
* **Speedup Factor Graph** (Page 11): Line graph showing the speedup multiplier of lock-free over lock-based implementation

(Note: results vary by hardware and scheduler.)

---

## Limitations

* Deferred reclamation is not fully lock-free
* Size tracking is approximate and used only for validation
* Single-file structure reflects academic submission constraints

---

## Skills Demonstrated

* C11 atomic operations and memory model awareness
* Lock-free algorithm implementation
* Concurrent testing and stress validation
* Performance benchmarking
* Systems-level debugging mindset

---

## Future Improvements

* Implement hazard pointers or epoch-based reclamation
* Split code into headers and modules
* Add linearizability testing
* Integrate thread sanitizers and formal tools

---

## License

MIT
