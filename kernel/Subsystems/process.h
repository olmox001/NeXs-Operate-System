/*
 * process.h - Task Control Block and Priority Scheduler Definitions
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Defines the Task Control Block (TCB) and scheduling parameters.
 * Implements a Mach/L4-inspired priority system with time-slice quanta.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "kernel.h"

// =============================================================================
// Security Levels (User IDs)
// =============================================================================
#define UID_KERNEL  0   // Ring 0: Internal Kernel Threads (Full Access)
#define UID_ROOT    1   // Ring 0: Administrative Tasks (Restricted Kernel API)
#define UID_USER    2   // Ring 3: User Space Applications (Future)

// =============================================================================
// Task State Machine
// =============================================================================
enum task_state {
    TASK_READY,         // Eligible for execution
    TASK_RUNNING,       // Currently executing on CPU
    TASK_SLEEPING,      // Suspended until a time duration expires
    TASK_WAITING_MSG,   // Blocked waiting for IPC message
    TASK_BLOCKED,       // Blocked on general I/O or mutex
    TASK_TERMINATED     // Processed finished, awaiting reclamation
};

// =============================================================================
// Priority Levels (0 to 255)
// =============================================================================
// Lower Number = Higher Priority (Logic inherited from Real-Time Systems)
#define PRIORITY_REALTIME   0   // Highest (0-30) - Hardware drivers
#define PRIORITY_SYSTEM     31  // System Services (31-62)
#define PRIORITY_HIGH       63  // Important Apps (63-126)
#define PRIORITY_NORMAL     127 // Standard User Apps (127-190)
#define PRIORITY_LOW        191 // Background Jobs (191-254)
#define PRIORITY_IDLE       255 // Idle Task (Only runs if nothing else)

// =============================================================================
// Time Quanta (ms)
// =============================================================================
// How many milliseconds a task runs before preemption.
// Higher priority tasks typically have smaller quanta for interactivity,
// or larger if they are batch compute? Actually, interactive = short.
// Here we assign smaller quanta to higher priorities to ensure responsiveness,
// but real-time tasks might preempt immediately anyway.
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
    // 1. CPU Context (Must be first for assembly offset stability)
    uint64_t  rsp;          // Stack Pointer (saved during switch)
    uint64_t  cr3;          // Page Directory (Virtual Memory Context)
    
    // 2. Identity
    uint32_t  pid;          // Process ID
    uint32_t  state;        // Current State enum
    
    // 3. Security
    uint8_t   uid;          // User ID
    uint8_t   gid;          // Group ID
    uint8_t   priority;     // Scheduling Priority
    uint8_t   flags;        // Task Flags
    
    // 4. Scheduling Metrics
    uint16_t  quantum;      // Time slices remaining in this turn
    uint16_t  base_quantum; // Reset value for quantum
    
    // 5. Timing Stats
    uint64_t  sleep_expiry; // Wakeup timestamp (if SLEEPING)
    uint64_t  cpu_time;     // Total ticks consumed
    uint64_t  start_time;   // Creation timestamp
    
    // 6. Resources
    void*     stack_base;   // Kernel Stack Base Address
    uint32_t  perm_mask;    // Permission Capability Mask
    
    // 7. List Management
    struct task* next;      // Next task in circular list
};

// Task Attributes Flags
#define TASK_FLAG_KERNEL    0x01
#define TASK_FLAG_SYSTEM    0x02
#define TASK_FLAG_BLOCKED   0x04
#define TASK_FLAG_DAEMON    0x08

// =============================================================================
// Scheduler API
// =============================================================================

// Initialization
void scheduler_init(void);

// Task Creation
struct task* task_create(void (*entry)(void)); // Default User Task
struct task* task_create_priority(void (*entry)(void), uint8_t priority);
struct task* task_create_full(void (*entry)(void), uint8_t priority, uint8_t uid);

// Yielding / Control
void schedule(void);
void yield(void);
void sleep(uint64_t ms);
void exit(void);

// Task Modification
void task_set_priority(struct task* t, uint8_t priority);
uint8_t task_get_priority(struct task* t);
void task_set_uid(struct task* t, uint8_t uid);
uint8_t task_get_uid(struct task* t);

// Context Switching (Assembly Helper)
// Takes current RSP, saves it to TCB, picks next task, returns next RSP.
uint64_t scheduler_switch(uint64_t current_rsp);

// Global Pointer to Running Task
// SMP: Current task is per-CPU. Used as accessor.
struct task* get_current_task(void);
// Macro for backwards compatibility (READ-ONLY access recommended outside scheduler)
#define current_task (get_current_task())

#endif // PROCESS_H
