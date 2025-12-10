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
#include "Subsystems/apic.h" // For lapic_id()
#include "Subsystems/spinlock.h"
#include "smp/smp.h" // For cpu_info_t

// =============================================================================
// Scheduler State
// =============================================================================
// SMP: Per-CPU State is now in smp.h (extren cpu_info_t cpus[])
static struct task* task_list = NULL;      // Head of circular task list
static uint32_t next_pid = 0;       // PID Counter

// Spinlock for SMP safety (Global Runqueue Lock)
static spinlock_t sched_lock;

// Accessor for external modules
struct task* get_current_task(void) {
    int id = lapic_id();
    // Safety check for early boot
    if (id < 0 || id >= MAX_CPUS) return NULL;
    return cpus[id].active_task;
}

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

// Helper for Idle Task Creation
static struct task* create_idle_task(void) {
    struct task* idle = buddy_alloc(sizeof(struct task));
    if (!idle) PANIC("scheduler: TCB alloc failed");
    
    memset(idle, 0, sizeof(struct task));
    
    // Allocate Stack for Idle Task (Safety)
    void* stack = buddy_alloc(TASK_STACK_SIZE);
    if (stack) {
        idle->stack_base = stack;
        
        // Initialize Stack Canary
        ((uint64_t*)stack)[0] = STACK_MAGIC;
        
        // Setup minimal stack (dummy) in case we switch TO it
        // But usually Idle loops are entered via function call on AP init.
        // We set RSP to top just in case.
        idle->rsp = (uint64_t)stack + TASK_STACK_SIZE; 
    }

    // Atomic PID Increment
    idle->pid = __sync_fetch_and_add(&next_pid, 1);
    idle->state = TASK_RUNNING;
    idle->uid = UID_KERNEL;
    idle->gid = 0;
    idle->priority = PRIORITY_IDLE;
    idle->quantum = get_quantum(PRIORITY_IDLE);
    idle->base_quantum = idle->quantum;
    idle->flags = TASK_FLAG_KERNEL;
    idle->perm_mask = 0xFFFFFFFF; // Full Permissions
    idle->start_time = timer_get_ticks();
    
    return idle;
}

void scheduler_init(void) {
    spinlock_init(&sched_lock);
    // memset(current_tasks, 0, sizeof(current_tasks)); // Removed

    // Create Idle Task for BSP (PID 0)
    struct task* idle = create_idle_task();
    
    // list_add(idle); // REMOVED: Idle tasks must be private to the CPU!
    
    // BSP is ID 0 (usually)
    int id = lapic_id();
    cpus[id].active_task = idle;
    cpus[id].idle_task = idle;
    cpus[id].id = id;
    
    vga_puts("DEBUG: Scheduler Initialized (BSP Idle PID 0)\n");
}

// SMP: Initialize Scheduler for an AP (Call from ap_main)
void scheduler_ap_init(void) {
    // Create Idle Task for this AP
    struct task* idle = create_idle_task();
    
    // spinlock_acquire(&sched_lock);
    // list_add(idle); // REMOVED: Idle tasks must be private to the CPU!
    // spinlock_release(&sched_lock);
    
    int id = lapic_id();
    cpus[id].active_task = idle;
    cpus[id].idle_task = idle;
    cpus[id].id = id;
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
    t->pid = __sync_fetch_and_add(&next_pid, 1);
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
    
    spinlock_acquire(&sched_lock);
    list_add(t);
    spinlock_release(&sched_lock);
    
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
    // SMP: Use accessor for reading
    struct task* cur = get_current_task();
    if (!cur) return;
    
    // Assigning to fields is OK via pointer
    cur->state = TASK_SLEEPING;
    cur->sleep_expiry = timer_get_ticks() + ms;
    
    yield();
}

/**
 * Main Context Switch Routine
 * Called from IRQ0 (Timer) or Yield ISR.
 * 
 * @param rsp Current Stack Pointer (context saved on stack)
 * @return New Stack Pointer (context to restore)
 */
// Main Context Switch (Called from ISR Stub)
uint64_t scheduler_switch(uint64_t rsp) {
    // 1. Scheduler Lock (Protect Global Task List)
    // In SMP, we MUST lock to traverse the list safely.
    if (spinlock_try_acquire(&sched_lock) != 0) {
         return rsp; // Contention: skip switch
    }
    
    int id = lapic_id(); // Local APIC ID
    
    // Safety check
    if (id < 0 || id >= MAX_CPUS) {
        spinlock_release(&sched_lock);
        return rsp;
    }

    struct task* cur = cpus[id].active_task; // Current on THIS CPU
    
    // Safety: Init logic should ensure cur is never NULL (Idle task)
    if (cur) {
        cur->rsp = rsp; // Save Context
        
        // Stack Check
        if (cur->stack_base && ((uint64_t*)cur->stack_base)[0] != STACK_MAGIC) {
            spinlock_release(&sched_lock);
            PANIC("Stack Overflow (Scheduler)");
        }
        
        // Timer Accounting
        if (cur->quantum > 0) cur->quantum--;
    }
    
    // 2. Pick Next Task
    struct task* best = NULL;
    struct task* t = NULL;
    
    // Start traversal from current->next to ensure fairness (Round Robin)
    if (task_list) {
        // Fix: logic to handle Idle task (whose next is NULL) or tasks not in list
        // If cur is valid and has a next, use it. Otherwise start from head.
        t = (cur && cur->next) ? cur->next : task_list;
        struct task* stop = t;
        
        do {
            // Wakeup Logic
            if (t->state == TASK_SLEEPING && timer_get_ticks() >= t->sleep_expiry) {
                t->state = TASK_READY;
                t->quantum = t->base_quantum;
            }
            
            // Selection Logic:
            // 1. Must be READY (or RUNNING on *this* CPU)
            // 2. Priority must be better (lower) than current best
            bool is_running_here = (t == cur);
            
            if (t->state == TASK_READY || is_running_here) {
                if (!best || t->priority < best->priority) {
                    best = t;
                }
            }
            
            t = t->next;
        } while (t != stop);
    }
    
    // Defaults to Idle Task if nothing found
    if (!best) {
        best = cpus[id].idle_task;
    }
    
    // 3. Preemption Check
    // If we picked 'cur' again (or nothing better), stay.
    if (best == cur) {
        if (cur->state != TASK_RUNNING) cur->state = TASK_RUNNING;
        spinlock_release(&sched_lock);
        return rsp;
    }
    
    // 4. Perform Switch
    // If 'cur' was Running, it becomes Ready
    if (cur && cur->state == TASK_RUNNING && cur != cpus[id].idle_task) {
        cur->state = TASK_READY;
    }
    
    // Mark 'best' as Running
    if (best != cpus[id].idle_task) {
        best->state = TASK_RUNNING;
        best->quantum = best->base_quantum;
    }
    
    // Assign to CPU
    cpus[id].active_task = best;
    
    spinlock_release(&sched_lock);
    
    // RSP Safety
    if (best->rsp == 0) PANIC("Switch to RSP 0");
    
    return best->rsp;
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
