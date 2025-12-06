/*
 * scheduler.c - Priority-Based Preemptive Scheduler
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "process.h"
#include "buddy.h"
#include "libc.h"
#include "idt.h"
#include "vga.h"
#include "handlers.h"

// Task Management
struct task* current_task = NULL;
static struct task* task_list = NULL;
static uint32_t next_pid = 0;
static volatile int sched_lock = 0;

// Stack Protection
#define STACK_MAGIC 0xDEADCAFEBABEBEEFull
#define TASK_STACK_SIZE 4096

// Quantum table by priority tier (ms values for 1000Hz timer)
static const uint16_t quantum_table[8] = {1, 5, 10, 20, 50, 75, 100, 200};

static uint16_t get_quantum(uint8_t priority) {
    return quantum_table[priority >> 5];
}

// Add task to circular list
static void list_add(struct task* t) {
    if (!task_list) {
        task_list = t;
        t->next = t;
    } else {
        struct task* tail = task_list;
        while (tail->next != task_list) tail = tail->next;
        tail->next = t;
        t->next = task_list;
    }
}

void scheduler_init(void) {
    struct task* idle = buddy_alloc(sizeof(struct task));
    if (!idle) PANIC("scheduler_init: alloc failed");
    memset(idle, 0, sizeof(struct task));
    
    idle->pid = next_pid++;
    idle->state = TASK_RUNNING;
    idle->uid = UID_KERNEL;
    idle->gid = 0;
    idle->priority = PRIORITY_IDLE;
    idle->quantum = get_quantum(PRIORITY_IDLE);
    idle->base_quantum = idle->quantum;
    idle->flags = TASK_FLAG_KERNEL;
    idle->perm_mask = 0xFFFFFFFF;
    idle->start_time = get_timer_ticks();
    
    list_add(idle);
    current_task = idle;
    vga_puts("DEBUG: Scheduler Initialized (PID 0)\n");
}

struct task* task_create_full(void (*entry)(void), uint8_t priority, uint8_t uid) {
    if (!entry) return NULL;
    
    struct task* t = buddy_alloc(sizeof(struct task));
    if (!t) return NULL;
    memset(t, 0, sizeof(struct task));
    
    void* stack = buddy_alloc(TASK_STACK_SIZE);
    if (!stack) { buddy_free(t); return NULL; }
    
    // Stack canary at bottom
    ((uint64_t*)stack)[0] = STACK_MAGIC;
    
    t->stack_base = stack;
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->uid = uid;
    t->gid = uid;
    t->priority = priority;
    t->quantum = get_quantum(priority);
    t->base_quantum = t->quantum;
    t->start_time = get_timer_ticks();
    
    // Default permissions by UID
    switch (uid) {
        case UID_KERNEL:
            t->perm_mask = 0xFFFFFFFF;
            t->flags = TASK_FLAG_KERNEL;
            break;
        case UID_ROOT:
            t->perm_mask = 0xFFFFFFFE; // All except PERM_KERNEL_MODE
            t->flags = TASK_FLAG_SYSTEM;
            break;
        default:
            t->perm_mask = 0x0000FFFF; // Basic user permissions
            break;
    }
    
    // Build interrupt return frame at stack top
    uint64_t stack_top = (uint64_t)stack + TASK_STACK_SIZE;
    uint64_t* sp = (uint64_t*)stack_top;
    
    *(--sp) = 0x10;
    *(--sp) = stack_top - 8;
    *(--sp) = 0x202;
    *(--sp) = 0x08;
    *(--sp) = (uint64_t)entry;
    *(--sp) = 0;
    *(--sp) = 0;
    for (int i = 0; i < 15; i++) *(--sp) = 0;
    *(--sp) = 0x10;
    *(--sp) = 0x10;
    *(--sp) = 0x10;
    *(--sp) = 0x10;
    
    t->rsp = (uint64_t)sp;
    list_add(t);
    return t;
}

struct task* task_create_priority(void (*entry)(void), uint8_t priority) {
    return task_create_full(entry, priority, UID_ROOT);
}

struct task* task_create(void (*entry)(void)) {
    return task_create_full(entry, PRIORITY_NORMAL, UID_USER);
}

void yield(void) {
    asm volatile("int $32");
}

void sleep(uint64_t ms) {
    if (!current_task) return;
    current_task->state = TASK_SLEEPING;
    current_task->sleep_expiry = get_timer_ticks() + ms;
    yield();
}

uint64_t scheduler_switch(uint64_t rsp) {
    if (!current_task || sched_lock) return rsp;
    sched_lock = 1;
    
    current_task->rsp = rsp;
    current_task->cpu_time++;
    
    // Stack canary check
    if (current_task->stack_base) {
        if (((uint64_t*)current_task->stack_base)[0] != STACK_MAGIC) {
            sched_lock = 0;
            PANIC("Stack overflow!");
        }
    }
    
    if (current_task->quantum > 0) current_task->quantum--;
    
    // Find next task
    struct task* best = NULL;
    struct task* t = current_task->next;
    uint64_t now = get_timer_ticks();
    
    do {
        if (t->state == TASK_SLEEPING && now >= t->sleep_expiry) {
            t->state = TASK_READY;
            t->quantum = t->base_quantum;
        }
        
        if (t->state == TASK_READY || t->state == TASK_RUNNING) {
            if (!best || t->priority < best->priority) {
                best = t;
            }
        }
        t = t->next;
    } while (t != current_task->next);
    
    if (current_task->state == TASK_RUNNING && 
        current_task->quantum > 0 &&
        (!best || current_task->priority <= best->priority)) {
        sched_lock = 0;
        return rsp;
    }
    
    if (!best) best = current_task;
    
    if (best != current_task) {
        if (current_task->state == TASK_RUNNING)
            current_task->state = TASK_READY;
        
        current_task = best;
        current_task->state = TASK_RUNNING;
        current_task->quantum = current_task->base_quantum;
    }
    
    sched_lock = 0;
    return current_task->rsp;
}

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
    while(1);
}

void schedule(void) { yield(); }
