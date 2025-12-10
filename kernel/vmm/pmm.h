/*
 * pmm.h - Physical Memory Manager (Layered Allocator)
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef PMM_H
#define PMM_H

#include "kernel.h"

// =============================================================================
// Configuration
// =============================================================================

#define PMM_PAGE_SIZE       4096
#define PMM_CACHE_SIZE      64      // Cached pages per CPU (Reduced to save BSS)
#define PMM_BATCH_SIZE      32      // Pages to transfer Global <-> Cache

// =============================================================================
// Public API
// =============================================================================

// Standard Allocation
void* pmm_alloc(size_t size);       // Size > 4KB not supported efficiently yet (uses fallback)
void  pmm_free(void* ptr);

// Single Page Optimized (compile-time constant check usually)
void* pmm_alloc_page(void);
void  pmm_free_page(void* ptr);

// Lifecycle
void pmm_init(void);
void pmm_enable_smp(void);

// Stats
void pmm_stats(size_t* total, size_t* used, size_t* free);

#endif // PMM_H
