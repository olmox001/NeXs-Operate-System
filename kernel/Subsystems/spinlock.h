/*
 * spinlock.h - SMP Atomic Synchronization Primitives
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "kernel.h"

// Spinlock Structure
typedef struct {
    volatile int locked; // 1 = Locked, 0 = Unlocked
    // Debug info could be added here (owner PID/CPU)
} spinlock_t;

// API
void spinlock_init(spinlock_t* lock);
void spinlock_acquire(spinlock_t* lock);
void spinlock_release(spinlock_t* lock);
int  spinlock_try_acquire(spinlock_t* lock);

#endif // SPINLOCK_H
