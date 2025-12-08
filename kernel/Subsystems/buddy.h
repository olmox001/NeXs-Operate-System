/*
 * buddy.h - Dynamic Buddy Memory Allocator Interface
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Implements a standard binary buddy allocator for managing physical/virtual memory.
 * Features:
 * - O(1) allocation and deallocation (amortized)
 * - Automatic coalescing of free blocks to reduce fragmentation
 * - Secure region isolation for sensitive keys
 */

#ifndef BUDDY_H
#define BUDDY_H

#include "kernel.h"

// =============================================================================
// Configuration
// =============================================================================

// Minimum Block Size: 4KB (1 Page)
// Maximum Block Size: 16MB (4KB << 11)
#define BUDDY_MIN_SIZE      4096
#define BUDDY_MAX_LEVELS    12      // Levels 0 to 11

// Memory Zones (for future expansion/DMA constraints)
#define ZONE_NORMAL     0   // Standard Heap (Kernel + User)
#define ZONE_SECURE     1   // Hidden Key Storage (not accessible via malloc)
#define ZONE_DMA        2   // Low Memory (<16MB) for Legacy DMA [Reserved]

// Secure Region Size (64KB Reserved)
// SECURE_REGION_SIZE is defined in kernel.h

// =============================================================================
// Public API
// =============================================================================

/**
 * Initialize Allocator (E820 Auto-Detection)
 * Parses the BIOS memory map to find the largest usable RAM region.
 * Reserves a portion for the Secure Region.
 * 
 * @param entries Pointer to E820 map array
 * @param count Number of entries in map
 * @param out_secure_base [Out] Returns the physical address of the secure region
 */
void buddy_init_e820(struct e820_entry* entries, int count, uint64_t* out_secure_base);

/**
 * Initialize Allocator (Explicit Range)
 * Manually sets the heap range. Used if E820 is unavailable or for testing.
 * 
 * @param start Start address of the heap
 * @param size Total size of the heap (bytes)
 */
void buddy_init(void* start, size_t size);

/**
 * Allocate Memory
 * Finds the smallest power-of-two block that fits the requested size.
 * 
 * @param size Number of bytes required
 * @return Pointer to allocated memory, or NULL if OOM
 */
void* buddy_alloc(size_t size);

/**
 * Free Memory
 * Returns the block to the free list and attempts to coalesce with its buddy.
 * 
 * @param ptr Pointer to memory returned by buddy_alloc
 */
void buddy_free(void* ptr);

/**
 * Get Allocator Statistics
 * 
 * @param total [Out] Total heap size
 * @param used [Out] Bytes currently allocated
 * @param free [Out] Bytes currently free
 */
void buddy_stats(size_t* total, size_t* used, size_t* free);

// =============================================================================
// Secure Region API (Bump Allocator)
// =============================================================================
// Used for SBLOCKs and encryption keys. Isolated from main heap.

void  secure_region_init(void* base, size_t size);
void* secure_alloc(size_t size);
void  secure_free(void* ptr); // No-op for bump allocator

// =============================================================================
// Global State Exports (Read-Only)
// =============================================================================
extern uint64_t g_heap_base;
extern uint64_t g_heap_size;
extern uint64_t g_secure_base;

#endif // BUDDY_H