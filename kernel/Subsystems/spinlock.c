/*
 * spinlock.c - SMP Atomic Synchronization Primitives
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "spinlock.h"

void spinlock_init(spinlock_t* lock) {
    lock->locked = 0;
}

void spinlock_acquire(spinlock_t* lock) {
    // Atomic Test-and-Set
    // while (locked == 1) spin;
    // Sets locked=1 and returns previous value.
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        // Spin-wait hint to CPU (optimizes pipeline)
        asm volatile("pause");
    }
    // Memory Barrier implied by __sync functions
}

void spinlock_release(spinlock_t* lock) {
    // Atomic Release
    __sync_lock_release(&lock->locked);
}

int spinlock_try_acquire(spinlock_t* lock) {
    // Returns 0 on success (acquired), 1 if busy
    // __sync_lock_test_and_set returns the PREVIOUS value.
    // So if it was 0 (unlocked), it returns 0 and sets it to 1. Success.
    // If it was 1 (locked), it returns 1. Fail.
    return __sync_lock_test_and_set(&lock->locked, 1) == 0;
}
