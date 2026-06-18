/*
 * scheduler.c - Priority-Based Preemptive Scheduler
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "process.h"
#include "buddy.h"
#include "libx.h"
#include "idt.h"
#include "vga.h"
#include "handlers.h"
#include "timer.h" // Ensures get_timer_ticks is available

// =============================================================================
// Scheduler State
// =============================================================================
struct task* current_task = NULL;   // Currently executing task
static struct task* task_list = NULL;      // Head of circular task list
static uint32_t next_pid = 0;       // PID Counter

// Spinlock for SMP safety (even on uniprocessor, good practice for IRQ reentrancy)
static volatile int sched_lock = 0;

// Stack Constants
#define STACK_MAGIC 0xDEADCAFEBABEBEEFull // Canary value
#define TASK_STACK_SIZE 4096              // 4KB Stack per task

// Quantum Lookup Table (Index = Priority Level >> 5)
// Maps priority bands to time slice durations in ms.
static const uint16_t quantum_table[8] = {
    1,   // 0-31    (Realtime)
    5,   // 32-63   (System)
    10,  // 64-95   (High)
    20,  // 96-127  (Normal)
    50,  // 128-159 (Low)
    75,  // 160-191
    100, // 192-223 (Idle/Background)
    200  // 224-255
};

// Map priority (0-255) to Quantum
static uint16_t get_quantum(uint8_t priority) {
    return quantum_table[priority >> 5];
}

// Add task to global circular list
static void list_add(struct task* t) {
    if (!task_list) {
        task_list = t;
        t->next = t; // Point to self
    } else {
        struct task* tail = task_list;
        while (tail->next != task_list) {
            tail = tail->next;
        }
        tail->next = t;
        t->next = task_list; // Close loop
    }
}

// =============================================================================
// Initialization
// =============================================================================

void scheduler_init(void) {
    // Create Idle Task (PID 0)
    // This is the formatted container for the infinite loop in kernel_main.
    struct task* idle = buddy_alloc(sizeof(struct task));
    if (!idle) PANIC("scheduler_init: TCB alloc failed");
    
    memset(idle, 0, sizeof(struct task));
    
    idle->pid = next_pid++;
    idle->state = TASK_RUNNING;
    idle->uid = UID_KERNEL;
    idle->gid = 0;
    idle->priority = PRIORITY_IDLE;
    idle->quantum = get_quantum(PRIORITY_IDLE);
    idle->base_quantum = idle->quantum;
    idle->flags = TASK_FLAG_KERNEL;
    idle->perm_mask = 0xFFFFFFFF; // Full Permissions
    idle->start_time = timer_get_ticks();
    
    list_add(idle);
    current_task = idle;
    
    vga_puts("DEBUG: Scheduler Initialized (PID 0 Active)\n");
}

// =============================================================================
// Task Creation
// =============================================================================

struct task* task_create_full(void (*entry)(void), uint8_t priority, uint8_t uid) {
    if (!entry) return NULL;
    
    // Allocate TCB
    struct task* t = buddy_alloc(sizeof(struct task));
    if (!t) return NULL;
    memset(t, 0, sizeof(struct task));
    
    // Allocate Stack
    void* stack = buddy_alloc(TASK_STACK_SIZE);
    if (!stack) { 
        buddy_free(t); 
        return NULL; 
    }
    
    // Set Stack Canary (Bottom of Stack)
    ((uint64_t*)stack)[0] = STACK_MAGIC;
    
    // Initialize Metadata
    t->stack_base = stack;
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->uid = uid;
    t->gid = uid;
    t->priority = priority;
    t->quantum = get_quantum(priority);
    t->base_quantum = t->quantum;
    t->start_time = timer_get_ticks();
    
    // Default Security Masks
    switch (uid) {
        case UID_KERNEL:
            t->perm_mask = 0xFFFFFFFF;
            t->flags = TASK_FLAG_KERNEL;
            break;
        case UID_ROOT:
            t->perm_mask = 0xFFFFFFFE; // All except PERM_KERNEL_MODE?
            t->flags = TASK_FLAG_SYSTEM;
            break;
        default:
            t->perm_mask = 0x0000FFFF; // User subset
            break;
    }
    
    // ===================================
    // Build Initial Stack Frame (Context)
    // ===================================
    // Emulate the stack state as if an interrupt just occurred.
    // When the scheduler switches to this task, it will 'iret' into 'entry'.
    
    uint64_t stack_top = (uint64_t)stack + TASK_STACK_SIZE;
    uint64_t* sp = (uint64_t*)stack_top;
    
    // 1. IRETQ Frame (SS, RSP, RFLAGS, CS, RIP)
    *(--sp) = 0x10;             // SS (Kernel Data)
    *(--sp) = stack_top - 8;    // RSP (Top of stack, minus alignment)
    *(--sp) = 0x202;            // RFLAGS (IF=1, Reserved=1) - Interrupts Enabled
    *(--sp) = 0x08;             // CS (Kernel Code)
    *(--sp) = (uint64_t)entry;  // RIP (Entry Point)
    
    // 2. Error Code / Int No (Dummy)
    *(--sp) = 0; // Err
    *(--sp) = 0; // Int No
    
    // 3. General Purpose Registers (GPRs)
    // Pops R15...RAX in common_stub
    for (int i = 0; i < 15; i++) *(--sp) = 0;
    
    // 4. Segment Selectors (GS, FS, ES, DS)
    *(--sp) = 0x10; // DS
    *(--sp) = 0x10; // ES
    *(--sp) = 0x10; // FS
    *(--sp) = 0x10; // GS
    
    t->rsp = (uint64_t)sp;
    
    list_add(t);
    return t;
}

// Wrapper: Priority Task
struct task* task_create_priority(void (*entry)(void), uint8_t priority) {
    return task_create_full(entry, priority, UID_ROOT);
}

// Wrapper: Standard User Task
struct task* task_create(void (*entry)(void)) {
    return task_create_full(entry, PRIORITY_NORMAL, UID_USER);
}

// =============================================================================
// Scheduling Logic
// =============================================================================

// Voluntarily yield CPU
void yield(void) {
    // Trigger Software Interrupt 32 (IRQ0 Vector) to force schedule
    // Or simpler: Trigger Context Switch via INT? No, usually just call switch.
    // But switch needs to be called from ISR.
    // We use INT 0x20 (32) which maps to IRQ0 handler, but explicitly.
    // Or syscall yield.
    asm volatile("int $32"); 
}

// Sleep for MS
void sleep(uint64_t ms) {
    if (!current_task) return;
    
    current_task->state = TASK_SLEEPING;
    current_task->sleep_expiry = timer_get_ticks() + ms;
    
    yield();
}

/**
 * Main Context Switch Routine
 * Called from IRQ0 (Timer) or Yield ISR.
 * 
 * @param rsp Current Stack Pointer (context saved on stack)
 * @return New Stack Pointer (context to restore)
 */
uint64_t scheduler_switch(uint64_t rsp) {
    if (!current_task || sched_lock) return rsp;
    
    sched_lock = 1;
    
    // 1. Save Context of Current Task
    current_task->rsp = rsp;
    current_task->cpu_time++;
    
    // Stack Safety Check
    if (current_task->stack_base) {
        if (((uint64_t*)current_task->stack_base)[0] != STACK_MAGIC) {
            sched_lock = 0;
            PANIC("Stack Overflow Detected");
        }
    }
    
    // Decrement Quantum
    if (current_task->quantum > 0) current_task->quantum--;
    
    // 2. Pick Next Task
    struct task* best = NULL;
    struct task* t = current_task->next;
    uint64_t now = timer_get_ticks();
    
    // One full loop through list
    do {
        // Wake up sleeping tasks
        if (t->state == TASK_SLEEPING && now >= t->sleep_expiry) {
            t->state = TASK_READY;
            t->quantum = t->base_quantum; // Refill quantum
        }
        
        // Candidate selection: READY or RUNNING
        if (t->state == TASK_READY || t->state == TASK_RUNNING) {
            // Priority Check (Lower value = Higher Priority)
            if (!best || t->priority < best->priority) {
                best = t;
            }
        }
        
        t = t->next;
    } while (t != current_task->next);
    
    // 3. Preemption Logic
    // If current is still valid and has quantum left, and no higher priority task exists...
    if (current_task->state == TASK_RUNNING && 
        current_task->quantum > 0 && 
        (!best || current_task->priority <= best->priority)) {
        
        sched_lock = 0;
        return rsp; // Continue running current
    }
    
    // If no candidate found (rare, idle task always exists), default to current
    if (!best) best = current_task;
    
    // 4. Perform Switch
    if (best != current_task) {
        if (current_task->state == TASK_RUNNING)
            current_task->state = TASK_READY;
        
        current_task = best;
        current_task->state = TASK_RUNNING;
        current_task->quantum = current_task->base_quantum; // Refill on switch-in
    }
    
    sched_lock = 0;
    return current_task->rsp;
}

// =============================================================================
// Helper API
// =============================================================================

struct task* scheduler_get_task(uint32_t pid) {
    if (!task_list) return NULL;

    struct task* t = task_list;
    do {
        if (t->pid == pid) return t;
        t = t->next;
    } while (t != task_list);

    return NULL;
}

void scheduler_wake(struct task* t) {
    if (t && (t->state == TASK_WAITING_MSG || t->state == TASK_SLEEPING || t->state == TASK_BLOCKED)) {
        t->state = TASK_READY;
        t->quantum = t->base_quantum; // Priority boost on wake?
    }
}

void scheduler_wait_msg(void) {
    if (!current_task) return;

    current_task->state = TASK_WAITING_MSG;
    yield();
}

// =============================================================================
// Task Accessors
// =============================================================================

void task_set_priority(struct task* t, uint8_t p) {
    if (t) {
        t->priority = p;
        t->base_quantum = get_quantum(p);
    }
}

uint8_t task_get_priority(struct task* t) {
    return t ? t->priority : PRIORITY_IDLE;
}

void task_set_uid(struct task* t, uint8_t uid) {
    if (t) t->uid = uid;
}

uint8_t task_get_uid(struct task* t) {
    return t ? t->uid : UID_USER;
}

void exit(void) {
    cli();
    if (current_task) current_task->state = TASK_TERMINATED;
    yield();
    while(1); // Should never reach here
}

void schedule(void) {
    yield();
}
