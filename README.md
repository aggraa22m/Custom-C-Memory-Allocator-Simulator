# Lock-Free Queue Implementation with Comprehensive Testing

**Author:** Ashish Aggrawal (0826524)  
**Course:** COIS 3320 - Fundamentals of Operating Systems

A high-performance concurrent queue implementation demonstrating the **Michael-Scott lock-free algorithm** using C11 atomics, validated through 210 comprehensive test cases and benchmarked against a traditional mutex-based baseline.

---

## 🎯 Project Overview

This project implements a **lock-free queue** data structure based on the Michael and Scott (1996) algorithm. The implementation uses C11 atomic operations to enable safe concurrent access without traditional mutex locks, achieving **~2.1x performance improvement** under high thread contention (32 threads) compared to lock-based synchronization.

### Key Achievements

- ✅ **210/210 test cases passed** (100% success rate)
- ✅ **2.1x speedup** at 32 threads vs mutex-based baseline
- ✅ Complete **FIFO ordering** preservation across all scenarios
- ✅ **Thread-safe** operation under high contention
- ✅ Comprehensive coverage of edge cases and boundary conditions

---

## 📚 Table of Contents

- [Background and Motivation](#background-and-motivation)
- [Theoretical Foundation](#theoretical-foundation)
- [Implementation Details](#implementation-details)
- [Test Design and Results](#test-design-and-results)
- [Performance Analysis](#performance-analysis)
- [Implementation Challenges](#implementation-challenges)
- [Build and Run](#build-and-run)
- [Future Improvements](#future-improvements)
- [References](#references)

---

## 🌟 Background and Motivation

### Why Lock-Free Data Structures?

Traditional lock-based synchronization mechanisms introduce several performance and correctness challenges:

- **Lock contention:** Multiple threads competing for the same lock create bottlenecks
- **Priority inversion:** Lower-priority threads holding locks delay higher-priority threads
- **Deadlock potential:** Improper lock ordering can lead to system-wide deadlocks
- **Convoying:** A slow thread holding a lock delays all waiting threads

**Lock-free algorithms** address these issues by allowing multiple threads to operate on shared data structures without mutual exclusion locks. Instead, they rely on atomic hardware primitives such as **compare-and-swap (CAS)** operations to coordinate concurrent access.

### Project Objectives

1. Implement the Michael and Scott lock-free queue algorithm in C using C11 atomic operations
2. Develop a lock-based queue implementation as a performance baseline
3. Design and execute comprehensive test cases to verify correctness
4. Evaluate performance characteristics across varying thread counts (1-32)
5. Analyze the trade-offs between lock-free and lock-based approaches

---

## 🧠 Theoretical Foundation

### The Michael and Scott Algorithm

The Michael and Scott lock-free queue algorithm, published in 1996, is one of the most widely studied and implemented concurrent queue algorithms. The algorithm maintains two atomic pointers:

- **Head pointer:** Points to a dummy node at the front of the queue
- **Tail pointer:** Points to the last node in the queue

#### Algorithm Properties

The algorithm guarantees **lock-freedom** through the following properties:

1. **Non-blocking progress:** At least one thread makes progress in a finite number of steps
2. **Linearizability:** Operations appear to occur atomically at some point between invocation and completion
3. **FIFO ordering:** Elements are dequeued in the same order they were enqueued

### Core Operations

#### Enqueue Operation

The enqueue operation follows these steps:

1. Allocate a new node with the value to be enqueued
2. Read the current tail pointer and its next pointer
3. Verify the tail pointer has not changed (consistency check)
4. If `tail.next` is NULL, attempt to link the new node using CAS
5. If successful, swing the tail pointer to the new node
6. If `tail.next` is not NULL, help advance the tail pointer

**The helping mechanism** is crucial for lock-freedom. When a thread observes that the tail pointer lags behind the actual last node, it assists by advancing the tail pointer before proceeding with its own operation.

#### Dequeue Operation

The dequeue operation follows these steps:

1. Read the current head, tail, and `head.next` pointers
2. Verify the head pointer has not changed (consistency check)
3. If head equals tail and `head.next` is NULL, the queue is empty
4. If head equals tail but `head.next` is not NULL, help advance the tail
5. Otherwise, read the value from `head.next`
6. Attempt to advance the head pointer using CAS
7. If successful, return the value

### The ABA Problem and Memory Reclamation

A significant challenge in lock-free programming is **safe memory reclamation**. The **ABA problem** occurs when:

1. Thread T1 reads pointer P with value A
2. Thread T2 changes P from A → B → A
3. Thread T1's CAS succeeds, believing nothing changed
4. The operation may corrupt data if A was freed and reallocated

#### Common Solutions

- **Hazard Pointers:** Threads announce which nodes they are accessing
- **Epoch-Based Reclamation:** Nodes are freed only after all threads have moved to a new epoch
- **Deferred Reclamation:** Nodes are added to a retired list and freed later

This implementation uses **deferred reclamation**, where dequeued nodes are added to a retired list and cleaned up after benchmark completion. This approach is simple and effective for testing purposes, though production systems would require more sophisticated memory management.

---

## 🔧 Implementation Details

### Data Structures

#### Node Structure

Each node contains an integer value and an atomic pointer to the next node. The atomic annotation ensures all accesses to the next pointer use proper memory ordering.

```c
typedef struct Node {
    int value;
    _Atomic(struct Node *) next;
} Node;
```

#### Lock-Free Queue Structure

The queue maintains atomic head and tail pointers, plus an atomic size counter for validation purposes. The size counter is not part of the original Michael and Scott algorithm but aids in testing.

```c
typedef struct {
    _Atomic(Node *) head;
    _Atomic(Node *) tail;
    _Atomic(int) size;
} LFQueue;
```

#### Lock-Based Queue Structure

The lock-based implementation uses a single mutex to protect all queue operations. This represents a straightforward baseline for performance comparison.

```c
typedef struct {
    Node *head;
    Node *tail;
    pthread_mutex_t lock;
    int size;
} LockedQueue;
```

### Lock-Free Queue Implementation

#### Initialization

The queue is initialized with a **dummy node** that both head and tail point to. This simplifies the algorithm by ensuring `head.next` always exists when the queue is non-empty.

```c
void lfqueue_init(LFQueue *q) {
    Node *dummy = new_node(0);
    atomic_init(&q->head, dummy);
    atomic_init(&q->tail, dummy);
    atomic_init(&q->size, 0);
}
```

#### Enqueue Implementation

The enqueue implementation uses atomic compare-and-swap operations to guarantee thread safety without locks, closely adhering to the Michael and Scott algorithm.

**Key features:**
- Multiple threads can safely attempt to enqueue simultaneously
- Only one thread succeeds at each CAS stage
- The **helping mechanism** (else branch) advances the tail pointer when observing `tail.next != NULL`
- Guarantees progress even if a thread is preempted between linking a node and updating the tail pointer

#### Dequeue Implementation

The dequeue operation carefully handles the case where the queue appears empty.

**Key features:**
- Consistency checks ensure `head == tail` and `next == NULL` indicate true emptiness
- No concurrent enqueue is in progress
- Memory reclamation is deferred by adding the old head node to a retired list
- Prevents use-after-free bugs if the node is still being accessed by another thread

### Lock-Based Queue Implementation

The lock-based implementation provides a straightforward comparison baseline. All operations acquire the mutex before accessing shared data, ensuring mutual exclusion.

**Trade-offs:**
- Conceptually simpler than the lock-free version
- The mutex introduces serialization points that limit scalability
- Performance degrades linearly with increased thread count

---

## ✅ Test Design and Results

### Test Strategy

The testing strategy encompasses three dimensions:

1. **Correctness Testing:** Verify FIFO ordering and edge case handling
2. **Concurrency Testing:** Ensure thread safety under various concurrent access patterns
3. **Performance Testing:** Measure throughput and scalability across thread counts

### Core Correctness Tests (10 Tests)

| Test # | Test Name | Description |
|--------|-----------|-------------|
| 1 | Empty Queue Dequeue | Confirms proper failure indicator when dequeuing from empty queue |
| 2 | Single Operation | Tests simplest functionality: enqueue and dequeue a single item |
| 3 | FIFO Order (10 items) | Enqueues numbers 0-9 and verifies they dequeue in same order |
| 4 | Bulk Operations (100 items) | Tests scalability with 100 items, verifying FIFO throughout |
| 5 | Alternating Operations | Interleaves enqueue/dequeue to test rapidly changing states |
| 6 | Concurrent Producers | 4 threads simultaneously enqueue 25 items each (100 total) |
| 7 | Concurrent Consumers | 4 threads simultaneously dequeue from pre-populated queue |
| 8 | Mixed Producers/Consumers | 8 threads perform random operations, verify final consistency |
| 9 | Stress Test (10,000 items) | Tests performance with large datasets |
| 10 | Lock-Based Correctness | Validates the mutex-based baseline implementation |

### Bonus Test Cases (200 Tests)

To achieve comprehensive coverage, 200 additional test cases were implemented across 10 test sets:

#### Test Set 1: FIFO with Varying Sizes (100 tests)
- Queue sizes from 10 to 1,000 items (increments of 10)
- Verifies FIFO ordering across a wide range of data sizes
- Identifies any size-dependent bugs

#### Test Set 2: Empty Dequeue Tests (10 tests)
- Repeated empty dequeue operations
- Ensures consistent behavior and prevents state corruption

#### Test Set 3: Single Item Tests (10 tests)
- Individual enqueue-dequeue cycles with various values
- Verifies value preservation and state management

#### Test Set 4: Alternating Operations (20 tests)
- Alternating patterns with increasing operation counts (5-100)
- Tests proper behavior across various queue depth patterns

#### Test Set 5: Boundary Value Tests (10 tests)
- Tests with negative numbers, zero, and large positive numbers
- Ensures the queue handles the entire range of integer values

#### Test Set 6: Rapid Operations (10 tests)
- Quickly enqueue 50 items, then quickly dequeue 50 items
- Emphasizes transitions between empty and non-empty states

#### Test Set 7: Bulk Operations (15 tests)
- Data sizes: 5, 10, 25, 50, 75, 100, 150, 200, 300, 400, 500, 750, 1,000, 2,000, 5,000 items
- Verifies correct behavior and performance at different scales

#### Test Set 8: Interleaved Operations (10 tests)
- Complex patterns like: enqueue 3 items, dequeue 2 items (repeated)
- Verifies proper behavior with irregular operation patterns

#### Test Set 9: Negative Value Stress Tests (5 tests)
- Focused testing with 100 negative values
- Ensures no issues with signed integer handling in concurrent contexts

#### Test Set 10: Sequential Access Patterns (10 tests)
- Sequential access patterns with increasing sizes (10-100 items)
- Confirms accuracy of sequential access patterns

### Test Results Summary

**✅ All 210 test cases passed successfully (100% pass rate)**

The implementation demonstrates:
- ✅ Correct FIFO ordering across all data sizes
- ✅ Thread safety under concurrent access
- ✅ Proper handling of edge cases and boundary values
- ✅ Scalability from small to large datasets
- ✅ Consistent behavior across diverse access patterns

---

## 📊 Performance Analysis

### Experimental Setup

Performance benchmarks were conducted with the following configuration:

- **Operations per thread:** 50,000 enqueue/dequeue operations
- **Operation distribution:** 50% enqueue, 50% dequeue (random selection)
- **Thread counts:** 1, 2, 4, 8, 16, 32
- **Pre-population:** Each queue pre-populated with 100 items to reduce empty-queue overhead
- **Timing:** Measured using `clock_gettime` with `CLOCK_MONOTONIC`

### Performance Results

#### Thread Scalability Results

| Thread Count | Lock-Based Time (s) | Lock-Free Time (s) | Speedup |
|--------------|---------------------|-----------------------|---------|
| 1            | 0.0026              | 0.0039                | 0.66x   |
| 2            | 0.0156              | 0.0117                | **1.34x** |
| 4            | 0.0495              | 0.0231                | **2.15x** |
| 8            | 0.0843              | 0.0463                | 1.82x   |
| 16           | 0.1814              | 0.0938                | 1.93x   |
| 32           | 0.4046              | 0.1931                | **2.10x** |

### Execution Time Comparison

![Execution Time Graph](images/execution-time-graph.png)
*Figure 1: Execution time comparison showing lock-based (red) vs lock-free (blue) performance across thread counts*

#### Key Observations:

- **Lock-Based (Red):** Degrades rapidly as thread count increases
  - At 32 threads: 0.4046 seconds
  - Shows near-linear performance degradation
  
- **Lock-Free (Blue):** Maintains relatively stable performance
  - At 32 threads: 0.1931 seconds
  - Superior resistance to contention

- **Single-Thread Overhead:** Lock-free has initial overhead (0.0039s vs 0.0026s) due to atomic operations

- **Multi-Threaded Performance:** Lock-free outperforms in every multi-threaded scenario (2+ threads)

### Speedup Analysis

![Speedup Graph](images/speedup-graph.png)
*Figure 2: Speedup factor of lock-free over lock-based implementation*

The speedup factor is calculated as: **Time_Locked / Time_LockFree**

#### Performance Characteristics:

- **Crossover Point (2 threads):** 1.34x speedup
  - Lock-free overcomes initial atomic overhead
  - Reduced contention begins to show benefits

- **Peak Efficiency (4 threads):** 2.15x speedup
  - Optimal balance between parallelism and contention
  - System has enough parallelism without serious memory contention

- **High Contention Stability (8-32 threads):** 1.82x - 2.10x speedup
  - Maintains ~2x speedup even at 32 threads
  - Demonstrates excellent scalability
  - Avoids severe convoying effects of mutex-based approach

### Detailed Analysis

#### Single-Thread Performance (0.66x)

The lock-free implementation performs worse in single-threaded scenarios for expected reasons:

1. **Atomic operation overhead:** Higher latency than non-atomic memory accesses
2. **Memory ordering constraints:** Atomic operations enforce stronger memory ordering guarantees
3. **Retry loops:** The algorithm may retry operations even without contention
4. **No amortization:** Lock acquisition cost can be amortized across multiple operations

**Conclusion:** This single-threaded overhead is the fundamental trade-off of lock-free algorithms—sacrificing single-threaded performance for better contention scalability.

#### Two-Thread Performance (1.34x)

At two threads, the lock-free implementation achieves 1.34x speedup:

- Atomic operation overhead begins to be offset by reduced contention
- Lock-based version experiences serialization as threads compete for mutex
- Lock-free version frequently permits both threads to advance simultaneously

#### Four-Thread Performance (2.15x - Peak)

Maximum relative performance at four threads indicates:

- System has sufficient parallelism to exploit lock-free design
- Not yet experiencing serious memory contention issues
- Optimal balance point for this hardware configuration

#### Eight to Thirty-Two Thread Performance (1.82x - 2.10x)

Performance stabilizes in this range due to:

- **Cache coherency overhead:** More threads increase cache line bouncing
- **Memory bus contention:** Multiple threads competing for memory bandwidth
- **CAS retry overhead:** Higher contention increases failed CAS attempts

**Key Finding:** The sustained ~2x speedup at 32 threads demonstrates good scalability. Lock-based version shows near-linear degradation, while lock-free maintains relatively stable performance.

### Comparison with Published Results

The observed performance characteristics align well with published research:

1. **Michael and Scott (1996):** Reported 2-3x speedup on contemporary hardware
2. **Moir et al. (2005):** Observed similar single-threaded overhead
3. **Herlihy and Shavit (2008):** Documented the trade-off between single-threaded overhead and multi-threaded scalability

**Conclusion:** The 2x speedup observed in this implementation is typical for lock-free queues.

### Theoretical Performance Limits

The theoretical maximum speedup is limited by:

1. **Amdahl's Law:** Speedup limited by serializable portion of workload
2. **Memory Bandwidth:** Shared memory bandwidth becomes the bottleneck
3. **Cache Coherency:** MESI protocol overhead increases with core count
4. **CAS Success Rate:** As contention increases, CAS success rate decreases

The observed plateau around 2x speedup suggests that **memory contention** has become the dominant factor.

#### Potential Improvements

Further improvements would require algorithmic changes such as:
- Combining operations to reduce memory traffic
- Using more sophisticated helping mechanisms
- Implementing queue segmentation or hierarchical designs

---

## 🚧 Implementation Challenges and Solutions

### Challenge 1: Memory Reclamation

**Problem:** Safely freeing dequeued nodes without causing use-after-free bugs or memory leaks.

**Solution:** Implemented a deferred reclamation strategy using a retired node list. Nodes are added to the list upon dequeuing and released once the benchmark is finished. While simple and adequate for testing, production use would require improvement.

**Alternative Approaches:**
- **Hazard pointers:** More complex but enables instant reclamation
- **Epoch-based reclamation:** Good balance between complexity and efficiency
- **Reference counting:** Simple but has its own synchronization overhead

### Challenge 2: ABA Problem

**Problem:** The ABA problem can cause CAS operations to succeed incorrectly when a pointer value changes from A → B → A.

**Solution:** Deferred reclamation prevents the reuse of node memory until all operations complete, effectively avoiding ABA in this implementation.

**Alternative Approaches:**
- **Tagged pointers:** Add version numbers to pointers
- **Double-width CAS:** Use 128-bit CAS on 64-bit systems
- **Hazard pointers:** Prevent premature reclamation

### Challenge 3: Complexity of Helping Mechanisms

**Problem:** The assistance mechanism adds complexity to enqueue and dequeue operations.

**Solution:** Carefully implemented tail advancement checks to ensure threads assist in moving the tail pointer when observing it lagging behind the actual last node.

**Trade-offs:** While necessary for lock-freedom, the helping mechanism increases the number of branches and memory accesses in the critical path.

### Challenge 4: Concurrent Correctness Testing

**Problem:** Concurrency bugs and race conditions are difficult to replicate and debug.

**Solution:** 
- Implemented diverse test cases covering various concurrency patterns
- Used atomic counters to monitor operations and confirm consistency
- Ran tests multiple times to boost confidence

**Limitations:** Thorough testing cannot ensure the absence of subtle race conditions. Formal verification would provide stronger assurances.

---

## 🏗️ Build and Run

### Requirements

- **Compiler:** GCC or Clang with C11 support
- **Libraries:** POSIX threads
- **OS:** Linux or macOS

### Compilation

```bash
gcc -std=c11 -O2 -pthread Project3.c -o lockfree_queue
```

### Execution

```bash
./lockfree_queue
```

### Expected Output

```
Running 210 test cases...
✅ All 210 tests passed successfully!

Performance Benchmarks:
Thread Count | Lock-Based | Lock-Free | Speedup
      1      |  0.0026s   |  0.0039s  |  0.66x
      2      |  0.0156s   |  0.0117s  |  1.34x
      4      |  0.0495s   |  0.0231s  |  2.15x
      8      |  0.0843s   |  0.0463s  |  1.82x
     16      |  0.1814s   |  0.0938s  |  1.93x
     32      |  0.4046s   |  0.1931s  |  2.10x
```

---

## 🔮 Future Improvements

### Memory Management
- Implement **hazard pointers** or **epoch-based reclamation** for production-ready memory safety
- Add support for dynamic node pool allocation

### Code Structure
- Split implementation into separate headers and modules
- Create library interface for reusable components

### Advanced Testing
- Add **linearizability testing** using formal verification tools
- Integrate **ThreadSanitizer** and **AddressSanitizer** for automated bug detection
- Implement property-based testing frameworks

### Performance Optimization
- Explore **queue segmentation** for better cache locality
- Implement **batching mechanisms** to reduce atomic operations
- Add **NUMA-aware** optimizations for multi-socket systems

### Algorithm Variants
- Implement **LCRQ (Locked Concurrent Ring Queue)** for comparison
- Add support for **bounded queues** with backpressure
- Explore **priority queue** variants

---

## 📖 Lessons Learned

### The Complexity of Lock-Free Programming
Lock-free algorithms require careful consideration of memory ordering and concurrent execution. The helping mechanism, while elegant, is significantly more complex than a simple mutex-protected queue.

### Testing Concurrent Code is Hard
Race conditions may only appear under certain timing conditions. Comprehensive testing across different thread counts and operation patterns is necessary, but cannot guarantee correctness.

### Performance Trade-offs are Workload-Dependent
The lock-free queue has overhead in low-contention situations but clear benefits in high-contention scenarios. The choice between lock-free or lock-based depends on expected workload characteristics.

### Memory Management is Critical
Safe memory reclamation is one of the most challenging aspects of lock-free programming. The reclamation strategy chosen significantly impacts both complexity and performance.

---

## 📝 Project Structure

```
Lock-free-Implementation/
├── Project3.c                    # Full implementation with tests and benchmarks
├── README.md                     # This file
├── Project3_AshishAggrawal.pdf  # Detailed project report
└── images/                       # Performance graphs and screenshots
    ├── execution-time-graph.png
    └── speedup-graph.png
```

---

## 🎓 Skills Demonstrated

- ✅ C11 atomic operations and memory model awareness
- ✅ Lock-free algorithm implementation from academic literature
- ✅ Concurrent testing and stress validation
- ✅ Performance benchmarking and analysis
- ✅ Systems-level debugging mindset
- ✅ Understanding of memory reclamation challenges
- ✅ Awareness of real-world pitfalls (ABA problem, linearizability)

---

## 📚 References

### Core Papers
1. Michael, M. M., & Scott, M. L. (1996). [Simple, fast, and practical non-blocking and blocking concurrent queue algorithms](https://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf). PODC '96.

2. Michael, M. M. (2004). [Hazard pointers: Safe memory reclamation for lock-free objects](https://doi.org/10.1109/TPDS.2004.8). IEEE TPDS, 15(6), 491-504.

3. Herlihy, M., & Shavit, N. (2008). *The Art of Multiprocessor Programming*. Morgan Kaufmann.

### Additional Resources
4. Fraser, K. (2004). [Practical lock-freedom](https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-579.pdf). University of Cambridge.

5. Preshing, J. (2012). [An Introduction to Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/).

6. Moir, M., Luchangco, V., & Herlihy, M. (2005). [Obstruction-free algorithms can be practically wait-free](https://people.csail.mit.edu/shanir/publications/DISC2005.pdf). DISC 2005.

**Full reference list available in [Project Report](Project3_AshishAggrawal.pdf) (pages 17-19)**

---

## 📄 License

MIT License - See LICENSE file for details

---



---

**⭐ If you found this project helpful, please consider starring the repository!**
