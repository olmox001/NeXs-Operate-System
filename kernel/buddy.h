// buddy.h - Buddy Allocator for Kernel Heap
#ifndef BUDDY_H
#define BUDDY_H

#include "kernel.h"

#define BUDDY_MIN_SIZE 4096        // 4KB minimum block

#define BUDDY_MAX_LEVELS 8         // 4KB to 512KB (2^12 to 2^19)
// NOTE: MUST match heap size passed to buddy_init

// Initialize buddy allocator with physical memory range
void buddy_init(void* start, size_t size);

// Allocate memory block (returns NULL on failure)
void* buddy_alloc(size_t size);

// Free memory block
void buddy_free(void* ptr);

// Get allocation statistics
void buddy_stats(size_t* total, size_t* used, size_t* free);

#endif // BUDDY_H