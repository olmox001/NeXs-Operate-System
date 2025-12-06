/*
 * process.h - Task Control Block and Priority Scheduler
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Scheduling based on Mach/L4 concepts:
 * - Priority levels (0 = highest, 255 = lowest)
 * - User levels (kernel/root/user)
 * - Time quantum per priority band
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "kernel.h"

// =============================================================================
// User Levels (Unix-like UID)
// =============================================================================
#define UID_KERNEL  0   // Ring 0: Full kernel access
#define UID_ROOT    1   // Ring 0: Administrative, no kernel internals
#define UID_USER    2   // Ring 3: Standard user (future)

// Task States
enum task_state {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_WAITING_MSG,
    TASK_BLOCKED,
    TASK_TERMINATED
};

// Priority Levels
#define PRIORITY_REALTIME   0
#define PRIORITY_SYSTEM     31
#define PRIORITY_HIGH       63
#define PRIORITY_NORMAL     127
#define PRIORITY_LOW        191
#define PRIORITY_IDLE       255

// Time Quantum (in ms, 1000Hz timer)
#define QUANTUM_REALTIME    1
#define QUANTUM_SYSTEM      5
#define QUANTUM_HIGH        10
#define QUANTUM_NORMAL      20
#define QUANTUM_LOW         50
#define QUANTUM_IDLE        100

// =============================================================================
// Task Control Block (TCB)
// =============================================================================
struct task {
    // Context (first for alignment)
    uint64_t  rsp;
    uint64_t  cr3;
    
    // Identity
    uint32_t  pid;
    uint32_t  state;
    
    // User/Group
    uint8_t   uid;          // User ID (0=kernel, 1=root, 2=user)
    uint8_t   gid;          // Group ID (future)
    uint8_t   priority;
    uint8_t   flags;
    
    // Scheduling
    uint16_t  quantum;      // Ticks remaining (larger for ms precision)
    uint16_t  base_quantum;
    
    // Timing
    uint64_t  sleep_expiry;
    uint64_t  cpu_time;
    uint64_t  start_time;   // When task was created
    
    // Resources
    void*     stack_base;
    uint32_t  perm_mask;
    
    // Linked List
    struct task* next;
};

// Task Flags
#define TASK_FLAG_KERNEL    0x01
#define TASK_FLAG_SYSTEM    0x02
#define TASK_FLAG_BLOCKED   0x04
#define TASK_FLAG_DAEMON    0x08

// =============================================================================
// API
// =============================================================================
void scheduler_init(void);
struct task* task_create(void (*entry)(void));
struct task* task_create_priority(void (*entry)(void), uint8_t priority);
struct task* task_create_full(void (*entry)(void), uint8_t priority, uint8_t uid);
void schedule(void);
void yield(void);
void sleep(uint64_t ms);
void exit(void);

void task_set_priority(struct task* t, uint8_t priority);
uint8_t task_get_priority(struct task* t);
void task_set_uid(struct task* t, uint8_t uid);
uint8_t task_get_uid(struct task* t);

// Globals
extern struct task* current_task;
uint64_t scheduler_switch(uint64_t current_rsp);

#endif // PROCESS_H
