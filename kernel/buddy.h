/*
 * buddy.h - Dynamic Buddy Allocator Interface
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef BUDDY_H
#define BUDDY_H

#include "kernel.h"

// Block sizes: 4KB to 16MB (12 levels)
#define BUDDY_MIN_SIZE      4096
#define BUDDY_MAX_LEVELS    12

// Memory Zones
#define ZONE_NORMAL     0   // Standard heap
#define ZONE_SECURE     1   // Hidden key storage (not in normal alloc)
#define ZONE_DMA        2   // Future: Low memory for DMA

// Initialize allocator with explicit range
void buddy_init(void* start, size_t size);

// Initialize using E820 memory map (finds largest usable region)
void buddy_init_e820(struct e820_entry* entries, int count, uint64_t* out_secure_base);

// Allocate memory
void* buddy_alloc(size_t size);

// Free memory
void buddy_free(void* ptr);

// Get statistics
void buddy_stats(size_t* total, size_t* used, size_t* free);

// Secure region allocator (hidden from normal buddy)
void  secure_region_init(void* base, size_t size);
void* secure_alloc(size_t size);
void  secure_free(void* ptr);

#endif // BUDDY_H