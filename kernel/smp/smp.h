/*
 * smp.h - Symmetric Multiprocessing Support
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef SMP_H
#define SMP_H

#include "kernel.h"

// =============================================================================
// Constants
// =============================================================================

#define TRAMPOLINE_ADDR     0x8000
#define MAX_CPUS            32

// =============================================================================
// Public API
// =============================================================================

// Initialize SMP (Boot APs)
void smp_init(void);

// AP Entry Point (Called from assembly trampoline)
void ap_main(void);

// Get total booted CPUs
int smp_get_cpu_count(void);

// Get current CPU ID (wrapper around LAPIC)
int smp_get_id(void);

// =============================================================================
// Per-CPU State
// =============================================================================

// Forward declaration
struct task;

typedef struct {
    int id;                     // LAPIC ID
    struct task* active_task;   // Currently running task
    struct task* idle_task;     // Idle task for this CPU
    int nest_count;             // Interrupt nesting level
    // Future: struct tss_entry* tss;
} cpu_info_t;

// Global Array of Per-CPU Data (Indexed by LAPIC ID)
extern cpu_info_t cpus[MAX_CPUS];

#endif // SMP_H
