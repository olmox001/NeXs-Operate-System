/*
 * pmm.c - Physical Memory Manager Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "pmm.h"
#include "buddy.h"  // Backend Allocator
#include "libx.h"   // for NULL
#include "Subsystems/apic.h" // For lapic_id()

// =============================================================================
// Per-CPU Cache Structure
// =============================================================================

typedef struct {
    void* pages[PMM_CACHE_SIZE];
    int count;
} pmm_cpu_cache_t;

// SMP: Array of caches (indexed by LAPIC ID)
static pmm_cpu_cache_t cpu_caches[256];
static bool smp_enabled = false;

void pmm_enable_smp(void) {
    smp_enabled = true;
}

// Helper to safely get current cache index
static pmm_cpu_cache_t* get_cpu_cache(void) {
    // Strict safety check:
    // If SMP is not explicitly enabled, we are the BSP (Single Threaded Boot).
    // Accessing LAPIC registers before they are mapped (in vmm_init) causes Page Faults.
    if (!smp_enabled) {
        return &cpu_caches[0];
    }
    
    // SMP Active: Use Per-CPU Cache via LAPIC ID
    return &cpu_caches[lapic_id()];
}

// =============================================================================
// Internal Helpers
// =============================================================================

// Refill the local cache from the global buddy allocator
static void pmm_cache_refill(pmm_cpu_cache_t* cache) {
    int count = 0;
    while (cache->count < PMM_CACHE_SIZE && count < PMM_BATCH_SIZE) {
        void* page = buddy_alloc(PMM_PAGE_SIZE);
        if (!page) break;
        
        cache->pages[cache->count++] = page;
        count++;
    }
}

// Flush the local cache to the global buddy allocator
static void pmm_cache_flush(pmm_cpu_cache_t* cache) {
    int count = 0;
    // Keep half the cache hot
    int target = PMM_CACHE_SIZE / 2;
    
    while (cache->count > target && count < PMM_BATCH_SIZE) {
        void* page = cache->pages[--cache->count];
        buddy_free(page); // Takes global lock
        count++;
    }
}

// =============================================================================
// Public API
// =============================================================================

void pmm_init(void) {
    memset(cpu_caches, 0, sizeof(cpu_caches));
    extern void vga_puts(const char*);
    vga_puts("DEBUG: pmm_init done\n");
    // Buddy init is handled by kernel_main before this
}

void* pmm_alloc_page(void) {
    pmm_cpu_cache_t* cache = get_cpu_cache();

    // 1. Fast Path
    if (cache->count > 0) {
        return cache->pages[--cache->count];
    }
    
    // 2. Slow Path
    pmm_cache_refill(cache);
    
    // Retry
    if (cache->count > 0) {
        return cache->pages[--cache->count];
    }
    
    // 3. Fallback
    return buddy_alloc(PMM_PAGE_SIZE);
}

void* pmm_alloc(size_t size) {
    if (size <= PMM_PAGE_SIZE) {
        return pmm_alloc_page();
    }
    return buddy_alloc(size);
}

void pmm_free_page(void* ptr) {
    if (!ptr) return;

    pmm_cpu_cache_t* cache = get_cpu_cache();

    // 1. Fast Path
    if (cache->count < PMM_CACHE_SIZE) {
        cache->pages[cache->count++] = ptr;
        return;
    }
    
    // 2. Slow Path
    pmm_cache_flush(cache);
    
    // Retry
    if (cache->count < PMM_CACHE_SIZE) {
        cache->pages[cache->count++] = ptr;
    } else {
        buddy_free(ptr);
    }
}

void pmm_free(void* ptr) {
    // Generic free always goes to buddy for now to be safe with sizes
    // TODO: Improve to handle pages if we knew size
    buddy_free(ptr);
}

void pmm_stats(size_t* total, size_t* used, size_t* free) {
    buddy_stats(total, used, free);
    
    // Adjust for all caches
    size_t cached_bytes = 0;
    for (int i = 0; i < 256; i++) {
        cached_bytes += (cpu_caches[i].count * PMM_PAGE_SIZE);
    }
    
    if (free) *free += cached_bytes;
    if (used) *used -= cached_bytes;
}
