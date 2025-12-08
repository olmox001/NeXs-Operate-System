/*
 * buddy.c - Dynamic Buddy Memory Allocator Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "buddy.h"
#include "libx.h"
#include "vga.h"

// =============================================================================
// Internal Structures
// =============================================================================

// Block Metadata Header (stored immediately before payload)
struct buddy_block {
    struct buddy_block* next;   // Next block in the free list (if free)
    uint32_t level;             // Block order (0 = MIN_SIZE, 1 = 2*MIN_SIZE...)
    uint32_t is_free;           // Status flag (1=Free, 0=Allocated)
    uint64_t magic;             // Corruption Detection (Magic Canary)
};

#define BLOCK_MAGIC 0xB0DD1C0FFEULL // "Budd1Coffee"

// Free Lists Array
// free_lists[i] points to the head of the linked list of free blocks at level i
static struct buddy_block* free_lists[BUDDY_MAX_LEVELS];

// Global Heap State
static void* heap_start = NULL;
static size_t heap_size = 0;
static size_t bytes_allocated = 0;

// Secure Region State (Separate linear allocator)
static void* secure_start = NULL;
static size_t secure_size = 0;
static size_t secure_used = 0;

// Exported Globals
uint64_t g_total_memory = 0;
uint64_t g_heap_base = 0;
uint64_t g_heap_size = 0;
uint64_t g_secure_base = 0;

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Convert block level to size in bytes.
 * Level 0 = BUDDY_MIN_SIZE
 * Level n = BUDDY_MIN_SIZE * 2^n
 */
static inline size_t level_to_size(uint32_t level) {
    return (size_t)BUDDY_MIN_SIZE << level;
}

/**
 * Calculate required level for a given size.
 * Rounds up to the nearest power of two plus the header size.
 */
static uint32_t size_to_level(size_t size) {
    // Add overhead for metadata header
    size += sizeof(struct buddy_block);
    
    uint32_t level = 0;
    size_t block_size = BUDDY_MIN_SIZE;
    
    // Scan upwards until fit found
    while (block_size < size && level < BUDDY_MAX_LEVELS - 1) {
        block_size <<= 1;
        level++;
    }
    return level;
}

/**
 * Find the "Buddy" of a block.
 * In a buddy system, a block's buddy is at (Address XOR BlockSize).
 */
static void* get_buddy(void* block, uint32_t level) {
    size_t block_size = level_to_size(level);
    uint64_t offset = (uint64_t)block - (uint64_t)heap_start;
    uint64_t buddy_offset = offset ^ block_size;
    return (void*)((uint64_t)heap_start + buddy_offset);
}

/**
 * Split a block into two smaller "buddies".
 * The first half is returned (implied), size is reduced.
 * The second half is added to the lower-level free list.
 */
static void split_block(struct buddy_block* block, uint32_t level) {
    if (level == 0) return; // Cannot split minimum block
    
    size_t half_size = level_to_size(level - 1);
    
    // Create the Buddy (Second Half)
    struct buddy_block* buddy = (struct buddy_block*)((uint64_t)block + half_size);
    buddy->level = level - 1;
    buddy->is_free = 1;
    buddy->magic = BLOCK_MAGIC;
    
    // Update Original (First Half)
    block->level = level - 1;
    
    // Insert Buddy into Free List
    buddy->next = free_lists[level - 1];
    free_lists[level - 1] = buddy;
    
    // Note: The original block 'block' is NOT inserted into the free list loop here,
    // because it is typically being prepared for allocation or further splitting.
    // The recursive logic in buddy_alloc handles this.
}

// =============================================================================
// Public Implementation
// =============================================================================

/**
 * Initialize with E820 Map
 */
void buddy_init_e820(struct e820_entry* entries, int count, uint64_t* out_secure_base) {
    vga_puts("DEBUG: buddy_init_e820\n");
    
    // Search for the largest contiguous USABLE memory region
    uint64_t best_base = 0;
    uint64_t best_size = 0;
    
    for (int i = 0; i < count; i++) {
        if (entries[i].type == E820_TYPE_USABLE) {
            uint64_t base = entries[i].base;
            uint64_t len = entries[i].length;
            
            // Safety Filter: Skip low memory & Kernel Code Area (< 2MB)
            if (base < 0x200000) {
                if (base + len > 0x200000) {
                    // Truncate start
                    len -= (0x200000 - base);
                    base = 0x200000;
                } else {
                    continue; // Entirely inside safe zone
                }
            }
            
            if (len > best_size) {
                best_base = base;
                best_size = len;
            }
        }
    }
    
    // Fallback if no map (or massive failure)
    if (best_size < 0x80000) { // < 512KB
        vga_puts("WARN: E820 Map unusable. Using fallback 1MB heap at 2MB.\n");
        best_base = 0x200000;
        best_size = 0x100000; // 1MB
    }
    
    // Carve out Secure Region from the END of the heap
    if (best_size > SECURE_REGION_SIZE * 2) {
        secure_size = SECURE_REGION_SIZE;
        secure_start = (void*)(best_base + best_size - secure_size);
        secure_used = 0;
        
        // Shrink main heap
        best_size -= secure_size;
        
        if (out_secure_base) {
            *out_secure_base = (uint64_t)secure_start;
        }
        g_secure_base = (uint64_t)secure_start;
    }
    
    // Initialize Main Heap
    buddy_init((void*)best_base, best_size);
    
    // Export Stats
    g_heap_base = best_base;
    g_heap_size = best_size;
}

/**
 * Core Initialization
 */
void buddy_init(void* start, size_t size) {
    vga_puts("DEBUG: buddy_init core\n");
    heap_start = start;
    heap_size = size;
    bytes_allocated = 0;
    
    // Clear lists
    for (int i = 0; i < BUDDY_MAX_LEVELS; i++) {
        free_lists[i] = NULL;
    }
    
    // Calculate the largest power-of-two block that fits
    uint32_t level = 0;
    size_t block_size = BUDDY_MIN_SIZE;
    
    // Find absolute max level
    while ((block_size << 1) <= size && level < BUDDY_MAX_LEVELS - 1) {
        block_size <<= 1;
        level++;
    }
    
    // Create the Initial Block covering the whole heap
    struct buddy_block* initial = (struct buddy_block*)start;
    initial->level = level;
    initial->is_free = 1;
    initial->magic = BLOCK_MAGIC;
    initial->next = NULL;
    
    // Add to free list
    free_lists[level] = initial;
}

/**
 * Allocation Routine
 */
void* buddy_alloc(size_t size) {
    if (size == 0) return NULL;
    
    uint32_t needed_level = size_to_level(size);
    
    // Find smallest available free block >= needed_level
    uint32_t level = needed_level;
    while (level < BUDDY_MAX_LEVELS && !free_lists[level]) {
        level++;
    }
    
    // OOM Check
    if (level >= BUDDY_MAX_LEVELS) return NULL;
    
    // Split blocks down to needed size
    struct buddy_block* block = free_lists[level];
    free_lists[level] = block->next; // Remove from list
    
    while (level > needed_level) {
        split_block(block, level);
        level--;
        // After split, 'block' is the left half, and at 'level-1'
        // The right half is already in free_lists[level-1].
        // We continue loop to split 'block' further if needed.
    }
    
    // Mark allocated
    block->is_free = 0;
    bytes_allocated += level_to_size(needed_level);
    
    // Return payload pointer (skipping header)
    return (void*)((uint64_t)block + sizeof(struct buddy_block));
}

/**
 * Deallocation Routine
 */
void buddy_free(void* ptr) {
    if (!ptr) return;
    
    // Recover Header
    struct buddy_block* block = (struct buddy_block*)((uint64_t)ptr - sizeof(struct buddy_block));
    
    // Sanity Check
    if (block->magic != BLOCK_MAGIC) {
        vga_puts("CRITICAL: Heap Corruption Detected in free()\n");
        return; // Security Panic?
    }
    
    block->is_free = 1;
    bytes_allocated -= level_to_size(block->level);
    
    // Coalescing Loop
    uint32_t level = block->level;
    while (level < BUDDY_MAX_LEVELS - 1) {
        struct buddy_block* buddy = get_buddy(block, level);
        
        // Check Bounds
        if ((uint64_t)buddy < (uint64_t)heap_start || 
            (uint64_t)buddy >= (uint64_t)heap_start + heap_size) {
            break;
        }
        
        // Can we merge?
        // Buddy must be Free and same Level
        if (!buddy->is_free || buddy->level != level) {
            break; 
        }
        
        // Remove buddy from free list
        // Linear scan required because it's a singly linked list (optimization: use doubly linked)
        struct buddy_block** pp = &free_lists[level];
        bool found = false;
        while (*pp) {
            if (*pp == buddy) {
                *pp = buddy->next; // Unlink
                found = true;
                break;
            }
            pp = &(*pp)->next;
        }
        
        if (!found) {
            // Should not happen if state is consistent
            break; 
        }
        
        // Merge: Address of merged block is min(block, buddy)
        if (buddy < block) {
            block = buddy;
        }
        
        block->level++;
        level++;
    }
    
    // Insert final block into free list
    block->next = free_lists[level];
    free_lists[level] = block;
}

/**
 * Get Heap Stats
 */
void buddy_stats(size_t* total, size_t* used, size_t* free) {
    if (total) *total = heap_size;
    if (used) *used = bytes_allocated;
    if (free) *free = heap_size - bytes_allocated;
}

// =============================================================================
// Secure Region Allocator (Bump Allocator)
// =============================================================================

void secure_region_init(void* base, size_t size) {
    secure_start = base;
    secure_size = size;
    secure_used = 0;
}

void* secure_alloc(size_t size) {
    // 16-byte alignment
    size = (size + 15) & ~15;
    
    if (!secure_start || secure_used + size > secure_size) {
        return NULL; // Secure region full
    }
    
    void* ptr = (void*)((uint64_t)secure_start + secure_used);
    secure_used += size;
    return ptr;
}

void secure_free(void* ptr) {
    (void)ptr; 
    // Bump allocators do not support freeing individual items.
    // Secure region is cleared only on reset/wipe.
}