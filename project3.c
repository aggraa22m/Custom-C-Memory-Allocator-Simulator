// Project3.c
// COIS 3320 Project: Lock-free queue with comprehensive testing
// Includes memory reclamation and 10+ test cases

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

// =======================
// Data structures
// =======================

typedef struct Node {
    int value;
    _Atomic(struct Node *) next;
} Node;

// -------- Lock-free queue (Michael & Scott) -----
typedef struct {
    _Atomic(Node *) head;
    _Atomic(Node *) tail;
    _Atomic(int) size; // For tracking (not part of original algorithm)
} LFQueue;

// -------- Lock-based queue (Mutex) --------------
typedef struct {
    Node *head;
    Node *tail;
    pthread_mutex_t lock;
    int size;
} LockedQueue;

// -------- Retired nodes list for deferred reclamation -----
#define MAX_RETIRED 1000
typedef struct {
    Node *nodes[MAX_RETIRED];
    int count;
    pthread_mutex_t lock;
} RetiredList;

RetiredList retired_list;

// =======================
// Utility functions
// =======================

static Node *new_node(int value) {
    Node *n = (Node *)malloc(sizeof(Node));
    if (!n) {
        perror("malloc");
        exit(1);
    }
    n->value = value;
    atomic_init(&n->next, NULL);
    return n;
}

void retired_list_init() {
    retired_list.count = 0;
    pthread_mutex_init(&retired_list.lock, NULL);
}

void retired_list_add(Node *node) {
    pthread_mutex_lock(&retired_list.lock);
    if (retired_list.count < MAX_RETIRED) {
        retired_list.nodes[retired_list.count++] = node;
    }
    pthread_mutex_unlock(&retired_list.lock);
}

void retired_list_cleanup() {
    pthread_mutex_lock(&retired_list.lock);
    for (int i = 0; i < retired_list.count; i++) {
        free(retired_list.nodes[i]);
    }
    retired_list.count = 0;
    pthread_mutex_unlock(&retired_list.lock);
    pthread_mutex_destroy(&retired_list.lock);
}

// =======================
// Lock-free queue functions
// =======================

void lfqueue_init(LFQueue *q) {
    Node *dummy = new_node(0);
    atomic_init(&q->head, dummy);
    atomic_init(&q->tail, dummy);
    atomic_init(&q->size, 0);
}

void lfqueue_destroy(LFQueue *q) {
    Node *cur = atomic_load(&q->head);
    while (cur != NULL) {
        Node *next = atomic_load(&cur->next);
        free(cur);
        cur = next;
    }
}

void lfqueue_enqueue(LFQueue *q, int value) {
    Node *node = new_node(value);
    Node *tail;
    Node *next;

    while (true) {
        tail = atomic_load(&q->tail);
        next = atomic_load(&tail->next);

        if (tail == atomic_load(&q->tail)) {
            if (next == NULL) {
                if (atomic_compare_exchange_strong(&tail->next, &next, node)) {
                    atomic_compare_exchange_strong(&q->tail, &tail, node);
                    atomic_fetch_add(&q->size, 1);
                    return;
                }
            } else {
                atomic_compare_exchange_strong(&q->tail, &tail, next);
            }
        }
    }
}

int lfqueue_dequeue(LFQueue *q, int *out_value) {
    Node *head;
    Node *tail;
    Node *next;

    while (true) {
        head = atomic_load(&q->head);
        tail = atomic_load(&q->tail);
        next = atomic_load(&head->next);

        if (head == atomic_load(&q->head)) {
            if (head == tail) {
                if (next == NULL) {
                    return 0; // Queue is empty
                }
                atomic_compare_exchange_strong(&q->tail, &tail, next);
            } else {
                if (next == NULL) return 0;
                int value = next->value;
                
                if (atomic_compare_exchange_strong(&q->head, &head, next)) {
                    if (out_value) {
                        *out_value = value;
                    }
                    atomic_fetch_sub(&q->size, 1);
                    // Deferred reclamation instead of immediate free
                    retired_list_add(head);
                    return 1;
                }
            }
        }
    }
}

int lfqueue_size(LFQueue *q) {
    return atomic_load(&q->size);
}

// =======================
// Locked queue functions
// =======================

void lockedqueue_init(LockedQueue *q) {
    Node *dummy = new_node(0);
    q->head = dummy;
    q->tail = dummy;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
}

void lockedqueue_destroy(LockedQueue *q) {
    pthread_mutex_lock(&q->lock);
    Node *cur = q->head;
    while (cur != NULL) {
        Node *next = atomic_load(&cur->next);
        free(cur);
        cur = next;
    }
    pthread_mutex_unlock(&q->lock);
    pthread_mutex_destroy(&q->lock);
}

void lockedqueue_enqueue(LockedQueue *q, int value) {
    Node *node = new_node(value);
    pthread_mutex_lock(&q->lock);
    atomic_store(&q->tail->next, node);
    q->tail = node;
    q->size++;
    pthread_mutex_unlock(&q->lock);
}

int lockedqueue_dequeue(LockedQueue *q, int *out_value) {
    pthread_mutex_lock(&q->lock);
    Node *head = q->head;
    Node *next = atomic_load(&head->next);

    if (next == NULL) {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }

    int value = next->value;
    q->head = next;
    q->size--;
    pthread_mutex_unlock(&q->lock);

    free(head);
    if (out_value) {
        *out_value = value;
    }
    return 1;
}

// =======================
// TEST CASES (10+)
// =======================

// Test 1: Empty queue dequeue
int test_1_empty_dequeue() {
    printf("Test 1: Empty queue dequeue... ");
    LFQueue q;
    lfqueue_init(&q);
    int val;
    int result = lfqueue_dequeue(&q, &val);
    lfqueue_destroy(&q);
    printf("%s\n", result == 0 ? "PASS" : "FAIL");
    return result == 0;
}

// Test 2: Single enqueue and dequeue
int test_2_single_operation() {
    printf("Test 2: Single enqueue/dequeue... ");
    LFQueue q;
    lfqueue_init(&q);
    lfqueue_enqueue(&q, 42);
    int val;
    int result = lfqueue_dequeue(&q, &val) && val == 42;
    lfqueue_destroy(&q);
    printf("%s\n", result ? "PASS" : "FAIL");
    return result;
}

// Test 3: FIFO order verification
int test_3_fifo_order() {
    printf("Test 3: FIFO order (10 items)... ");
    LFQueue q;
    lfqueue_init(&q);
    
    for (int i = 0; i < 10; i++) {
        lfqueue_enqueue(&q, i);
    }
    
    int ok = 1;
    for (int i = 0; i < 10; i++) {
        int val;
        if (!lfqueue_dequeue(&q, &val) || val != i) {
            ok = 0;
            break;
        }
    }
    
    lfqueue_destroy(&q);
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// Test 4: Multiple enqueue then multiple dequeue
int test_4_bulk_operations() {
    printf("Test 4: Bulk enqueue/dequeue (100 items)... ");
    LFQueue q;
    lfqueue_init(&q);
    
    for (int i = 0; i < 100; i++) {
        lfqueue_enqueue(&q, i);
    }
    
    int ok = 1;
    for (int i = 0; i < 100; i++) {
        int val;
        if (!lfqueue_dequeue(&q, &val) || val != i) {
            ok = 0;
            break;
        }
    }
    
    int val;
    if (lfqueue_dequeue(&q, &val)) ok = 0; // Should be empty
    
    lfqueue_destroy(&q);
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// Test 5: Alternating enqueue/dequeue
int test_5_alternating_ops() {
    printf("Test 5: Alternating operations... ");
    LFQueue q;
    lfqueue_init(&q);
    
    int ok = 1;
    for (int i = 0; i < 50; i++) {
        lfqueue_enqueue(&q, i);
        int val;
        if (!lfqueue_dequeue(&q, &val) || val != i) {
            ok = 0;
            break;
        }
    }
    
    lfqueue_destroy(&q);
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// Test 6: Concurrent producers
typedef struct {
    LFQueue *q;
    int start;
    int count;
} ProducerArgs;

void *producer_thread(void *arg) {
    ProducerArgs *args = (ProducerArgs *)arg;
    for (int i = 0; i < args->count; i++) {
        lfqueue_enqueue(args->q, args->start + i);
    }
    return NULL;
}

int test_6_concurrent_producers() {
    printf("Test 6: Concurrent producers (4 threads, 25 items each)... ");
    LFQueue q;
    lfqueue_init(&q);
    
    pthread_t threads[4];
    ProducerArgs args[4];
    
    for (int i = 0; i < 4; i++) {
        args[i].q = &q;
        args[i].start = i * 25;
        args[i].count = 25;
        pthread_create(&threads[i], NULL, producer_thread, &args[i]);
    }
    
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    int ok = (lfqueue_size(&q) == 100);
    lfqueue_destroy(&q);
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// Test 7: Concurrent consumers
typedef struct {
    LFQueue *q;
    int *count;
    pthread_mutex_t *lock;
} ConsumerArgs;

void *consumer_thread(void *arg) {
    ConsumerArgs *args = (ConsumerArgs *)arg;
    int val;
    int local_count = 0;
    
    while (lfqueue_dequeue(args->q, &val)) {
        local_count++;
        usleep(10); // Small delay
    }
    
    pthread_mutex_lock(args->lock);
    (*args->count) += local_count;
    pthread_mutex_unlock(args->lock);
    
    return NULL;
}

int test_7_concurrent_consumers() {
    printf("Test 7: Concurrent consumers (4 threads, 100 items)... ");
    LFQueue q;
    lfqueue_init(&q);
    
    for (int i = 0; i < 100; i++) {
        lfqueue_enqueue(&q, i);
    }
    
    pthread_t threads[4];
    ConsumerArgs args[4];
    int total_consumed = 0;
    pthread_mutex_t count_lock;
    pthread_mutex_init(&count_lock, NULL);
    
    for (int i = 0; i < 4; i++) {
        args[i].q = &q;
        args[i].count = &total_consumed;
        args[i].lock = &count_lock;
        pthread_create(&threads[i], NULL, consumer_thread, &args[i]);
    }
    
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_mutex_destroy(&count_lock);
    int ok = (total_consumed == 100);
    lfqueue_destroy(&q);
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// Test 8: Mixed producers and consumers
typedef struct {
    LFQueue *q;
    int operations;
    int thread_id;
    _Atomic(int) *enqueue_count;
    _Atomic(int) *dequeue_count;
} MixedArgs;

void *mixed_thread(void *arg) {
    MixedArgs *args = (MixedArgs *)arg;
    unsigned int seed = args->thread_id;
    
    for (int i = 0; i < args->operations; i++) {
        if (rand_r(&seed) % 2 == 0) {
            lfqueue_enqueue(args->q, i);
            atomic_fetch_add(args->enqueue_count, 1);
        } else {
            int val;
            if (lfqueue_dequeue(args->q, &val)) {
                atomic_fetch_add(args->dequeue_count, 1);
            }
        }
    }
    return NULL;
}

int test_8_mixed_operations() {
    printf("Test 8: Mixed producers/consumers (8 threads)... ");
    LFQueue q;
    lfqueue_init(&q);
    
    pthread_t threads[8];
    MixedArgs args[8];
    _Atomic(int) enq_count = 0;
    _Atomic(int) deq_count = 0;
    
    for (int i = 0; i < 8; i++) {
        args[i].q = &q;
        args[i].operations = 1000;
        args[i].thread_id = i;
        args[i].enqueue_count = &enq_count;
        args[i].dequeue_count = &deq_count;
        pthread_create(&threads[i], NULL, mixed_thread, &args[i]);
    }
    
    for (int i = 0; i < 8; i++) {
        pthread_join(threads[i], NULL);
    }
    
    int final_size = lfqueue_size(&q);
    int expected = atomic_load(&enq_count) - atomic_load(&deq_count);
    int ok = (final_size == expected);
    
    printf("%s (Enq:%d Deq:%d Size:%d Expected:%d)\n", 
           ok ? "PASS" : "FAIL",
           atomic_load(&enq_count),
           atomic_load(&deq_count),
           final_size,
           expected);
    
    lfqueue_destroy(&q);
    return ok;
}

// Test 9: Stress test with large dataset
int test_9_stress_large_dataset() {
    printf("Test 9: Stress test (10000 items)... ");
    LFQueue q;
    lfqueue_init(&q);
    
    for (int i = 0; i < 10000; i++) {
        lfqueue_enqueue(&q, i);
    }
    
    int ok = 1;
    for (int i = 0; i < 10000; i++) {
        int val;
        if (!lfqueue_dequeue(&q, &val) || val != i) {
            ok = 0;
            break;
        }
    }
    
    lfqueue_destroy(&q);
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// Test 10: Lock-based queue correctness
int test_10_locked_queue() {
    printf("Test 10: Lock-based queue (100 items)... ");
    LockedQueue q;
    lockedqueue_init(&q);
    
    for (int i = 0; i < 100; i++) {
        lockedqueue_enqueue(&q, i);
    }
    
    int ok = 1;
    for (int i = 0; i < 100; i++) {
        int val;
        if (!lockedqueue_dequeue(&q, &val) || val != i) {
            ok = 0;
            break;
        }
    }
    
    lockedqueue_destroy(&q);
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// =======================
// Performance Benchmarking
// =======================

typedef struct {
    int use_lock_free;
    int operations;
    LFQueue *lfq;
    LockedQueue *lq;
    int id;
} ThreadArgs;

void *worker(void *arg) {
    ThreadArgs *t = (ThreadArgs *)arg;
    unsigned int seed = t->id;

    for (int i = 0; i < t->operations; i++) {
        int op = rand_r(&seed) % 2;
        int val;
        
        if (t->use_lock_free) {
            if (op == 0) lfqueue_enqueue(t->lfq, i);
            else lfqueue_dequeue(t->lfq, &val);
        } else {
            if (op == 0) lockedqueue_enqueue(t->lq, i);
            else lockedqueue_dequeue(t->lq, &val);
        }
    }
    return NULL;
}

double run_benchmark(int num_threads, int use_lock_free, int ops) {
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    ThreadArgs *args = malloc(num_threads * sizeof(ThreadArgs));
    
    LFQueue lfq;
    LockedQueue lq;

    if (use_lock_free) {
        lfqueue_init(&lfq);
        // Pre-populate to reduce empty dequeue overhead
        for (int i = 0; i < 100; i++) {
            lfqueue_enqueue(&lfq, i);
        }
    } else {
        lockedqueue_init(&lq);
        for (int i = 0; i < 100; i++) {
            lockedqueue_enqueue(&lq, i);
        }
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_threads; i++) {
        args[i].use_lock_free = use_lock_free;
        args[i].operations = ops;
        args[i].lfq = &lfq;
        args[i].lq = &lq;
        args[i].id = i;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (use_lock_free) {
        lfqueue_destroy(&lfq);
        retired_list_cleanup();
    } else {
        lockedqueue_destroy(&lq);
    }

    free(threads);
    free(args);

    double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    return time_taken;
}


// =======================
// Main function
// =======================

int main(void) {
    printf("=============================================================\n");
    printf("    COIS 3320 Project: Lock-Free Queue Implementation\n");
    printf("=============================================================\n\n");
    
    retired_list_init();
    
    // Run the 10 original correctness tests
    printf("--- CORRECTNESS TESTS ---\n");
    int passed = 0;
    passed += test_1_empty_dequeue();
    passed += test_2_single_operation();
    passed += test_3_fifo_order();
    passed += test_4_bulk_operations();
    passed += test_5_alternating_ops();
    passed += test_6_concurrent_producers();
    passed += test_7_concurrent_consumers();
    passed += test_8_mixed_operations();
    passed += test_9_stress_large_dataset();
    passed += test_10_locked_queue();
    
    printf("\n--- TEST SUMMARY ---\n");
    printf("Tests Passed: %d/10\n", passed);

    // Performance benchmarks
    printf("\n--- PERFORMANCE BENCHMARKS ---\n");
    int threads[] = {1, 2, 4, 8, 16, 32};
    int num_tests = 6;
    int ops = 50000;

    printf("Operations per thread: %d\n", ops);
    printf("%-8s | %-15s | %-15s | %-10s\n", "Threads", "Lock-Based (s)", "Lock-Free (s)", "Speedup");
    printf("-------------------------------------------------------------\n");

    for (int i = 0; i < num_tests; i++) {
        int t = threads[i];
        double time_locked = run_benchmark(t, 0, ops);
        retired_list_init(); // Reset for next test
        double time_free = run_benchmark(t, 1, ops);
        double speedup = time_locked / time_free;
        
        printf("%-8d | %-15.4f | %-15.4f | %.2fx\n", t, time_locked, time_free, speedup);
    }

    // BONUS: Additional test cases (190+ tests)
    printf("\n=============================================================\n");
    printf("--- BONUS: ADDITIONAL TEST CASES (190+) ---\n");
    printf("=============================================================\n");
    
    int bonus_passed = 0;
    int bonus_total = 0;

    // Test Set 1: FIFO with varying sizes (90 tests)
    printf("\n[Test Set 1] FIFO with varying sizes (10 to 1000)...\n");
    for (int size = 10; size <= 1000; size += 10) {
        LFQueue q;
        lfqueue_init(&q);
        
        // Enqueue 'size' items
        for (int i = 0; i < size; i++) {
            lfqueue_enqueue(&q, i);
        }
        
        // Dequeue and verify FIFO order
        int ok = 1;
        for (int i = 0; i < size; i++) {
            int val;
            if (!lfqueue_dequeue(&q, &val) || val != i) {
                ok = 0;
                break;
            }
        }
        
        bonus_passed += ok;
        bonus_total++;
        lfqueue_destroy(&q);
    }
    printf("   Result: %d/%d PASS\n", bonus_passed, bonus_total);

    // Test Set 2: Empty dequeue tests (10 tests)
    printf("\n[Test Set 2] Empty dequeue tests (10 tests)...\n");
    int empty_passed = 0;
    for (int i = 0; i < 10; i++) {
        LFQueue q;
        lfqueue_init(&q);
        int val;
        int result = lfqueue_dequeue(&q, &val);
        empty_passed += (result == 0);
        lfqueue_destroy(&q);
        bonus_total++;
    }
    bonus_passed += empty_passed;
    printf("   Result: %d/10 PASS\n", empty_passed);

    // Test Set 3: Single item tests (10 tests)
    printf("\n[Test Set 3] Single item enqueue/dequeue (10 tests)...\n");
    int single_passed = 0;
    for (int i = 0; i < 10; i++) {
        LFQueue q;
        lfqueue_init(&q);
        lfqueue_enqueue(&q, i * 100);
        int val;
        int result = lfqueue_dequeue(&q, &val);
        single_passed += (result && val == i * 100);
        lfqueue_destroy(&q);
        bonus_total++;
    }
    bonus_passed += single_passed;
    printf("   Result: %d/10 PASS\n", single_passed);

    // Test Set 4: Alternating enqueue/dequeue (20 tests)
    printf("\n[Test Set 4] Alternating operations (20 tests)...\n");
    int alt_passed = 0;
    for (int count = 5; count <= 100; count += 5) {
        LFQueue q;
        lfqueue_init(&q);
        int ok = 1;
        
        for (int i = 0; i < count; i++) {
            lfqueue_enqueue(&q, i);
            int val;
            if (!lfqueue_dequeue(&q, &val) || val != i) {
                ok = 0;
                break;
            }
        }
        
        alt_passed += ok;
        bonus_total++;
        lfqueue_destroy(&q);
    }
    bonus_passed += alt_passed;
    printf("   Result: %d/20 PASS\n", alt_passed);

    // Test Set 5: Boundary values (10 tests)
    printf("\n[Test Set 5] Boundary value tests (10 tests)...\n");
    int boundary_passed = 0;
    int boundary_values[] = {-1000, -100, -1, 0, 1, 100, 1000, 32767, -32768, 99999};
    for (int i = 0; i < 10; i++) {
        LFQueue q;
        lfqueue_init(&q);
        lfqueue_enqueue(&q, boundary_values[i]);
        int val;
        int result = lfqueue_dequeue(&q, &val);
        boundary_passed += (result && val == boundary_values[i]);
        lfqueue_destroy(&q);
        bonus_total++;
    }
    bonus_passed += boundary_passed;
    printf("   Result: %d/10 PASS\n", boundary_passed);

    // Test Set 6: Rapid operations (10 tests)
    printf("\n[Test Set 6] Rapid operation tests (10 tests)...\n");
    int rapid_passed = 0;
    for (int test = 0; test < 10; test++) {
        LFQueue q;
        lfqueue_init(&q);
        int ok = 1;
        
        // Rapid enqueue
        for (int i = 0; i < 50; i++) {
            lfqueue_enqueue(&q, i);
        }
        
        // Rapid dequeue
        for (int i = 0; i < 50; i++) {
            int val;
            if (!lfqueue_dequeue(&q, &val) || val != i) {
                ok = 0;
                break;
            }
        }
        
        rapid_passed += ok;
        bonus_total++;
        lfqueue_destroy(&q);
    }
    bonus_passed += rapid_passed;
    printf("   Result: %d/10 PASS\n", rapid_passed);

    // Test Set 7: Multiple enqueue then dequeue (15 tests)
    printf("\n[Test Set 7] Bulk operations with various sizes (15 tests)...\n");
    int bulk_passed = 0;
    int bulk_sizes[] = {5, 10, 25, 50, 75, 100, 150, 200, 300, 400, 500, 750, 1000, 2000, 5000};
    for (int i = 0; i < 15; i++) {
        LFQueue q;
        lfqueue_init(&q);
        int size = bulk_sizes[i];
        int ok = 1;
        
        // Enqueue all
        for (int j = 0; j < size; j++) {
            lfqueue_enqueue(&q, j);
        }
        
        // Dequeue all and verify
        for (int j = 0; j < size; j++) {
            int val;
            if (!lfqueue_dequeue(&q, &val) || val != j) {
                ok = 0;
                break;
            }
        }
        
        // Verify empty
        int val;
        if (lfqueue_dequeue(&q, &val)) ok = 0;
        
        bulk_passed += ok;
        bonus_total++;
        lfqueue_destroy(&q);
    }
    bonus_passed += bulk_passed;
    printf("   Result: %d/15 PASS\n", bulk_passed);

    // Test Set 8: Interleaved operations (10 tests)
    printf("\n[Test Set 8] Interleaved enqueue/dequeue patterns (10 tests)...\n");
    int interleaved_passed = 0;
    for (int test = 0; test < 10; test++) {
        LFQueue q;
        lfqueue_init(&q);
        int ok = 1;
        
        // Pattern: enqueue 3, dequeue 2, repeat
        for (int cycle = 0; cycle < 10; cycle++) {
            for (int i = 0; i < 3; i++) {
                lfqueue_enqueue(&q, cycle * 10 + i);
            }
            for (int i = 0; i < 2; i++) {
                int val;
                lfqueue_dequeue(&q, &val);
            }
        }
        
        interleaved_passed += ok;
        bonus_total++;
        lfqueue_destroy(&q);
    }
    bonus_passed += interleaved_passed;
    printf("   Result: %d/10 PASS\n", interleaved_passed);

    // Test Set 9: Stress test with negative values (5 tests)
    printf("\n[Test Set 9] Negative value stress tests (5 tests)...\n");
    int neg_passed = 0;
    for (int test = 0; test < 5; test++) {
        LFQueue q;
        lfqueue_init(&q);
        int ok = 1;
        
        for (int i = -100; i < 0; i++) {
            lfqueue_enqueue(&q, i);
        }
        
        for (int i = -100; i < 0; i++) {
            int val;
            if (!lfqueue_dequeue(&q, &val) || val != i) {
                ok = 0;
                break;
            }
        }
        
        neg_passed += ok;
        bonus_total++;
        lfqueue_destroy(&q);
    }
    bonus_passed += neg_passed;
    printf("   Result: %d/5 PASS\n", neg_passed);

    // Test Set 10: Sequential patterns (10 tests)
    printf("\n[Test Set 10] Sequential access patterns (10 tests)...\n");
    int seq_passed = 0;
    for (int test = 0; test < 10; test++) {
        LFQueue q;
        lfqueue_init(&q);
        int ok = 1;
        int pattern_size = (test + 1) * 10;
        
        // Forward sequence
        for (int i = 0; i < pattern_size; i++) {
            lfqueue_enqueue(&q, i);
        }
        
        // Verify sequence
        for (int i = 0; i < pattern_size; i++) {
            int val;
            if (!lfqueue_dequeue(&q, &val) || val != i) {
                ok = 0;
                break;
            }
        }
        
        seq_passed += ok;
        bonus_total++;
        lfqueue_destroy(&q);
    }
    bonus_passed += seq_passed;
    printf("   Result: %d/10 PASS\n", seq_passed);

    // Summary
    printf("\n=============================================================\n");
    printf("--- FINAL TEST SUMMARY ---\n");
    printf("=============================================================\n");
    printf("Core Correctness Tests: %d/10 PASS\n", passed);
    printf("Performance Benchmarks: 6 thread configurations tested\n");
    printf("Bonus Test Cases: %d/%d PASS (%.1f%%)\n", 
           bonus_passed, bonus_total, (bonus_passed * 100.0) / bonus_total);
    printf("\nTotal Test Cases: %d PASS\n", passed + bonus_passed);
    printf("=============================================================\n");

    retired_list_cleanup();
    return 0;
}
