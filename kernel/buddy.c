/*
 * buddy.c - Dynamic Buddy Memory Allocator
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "buddy.h"
#include "libc.h"
#include "vga.h"

// Block Metadata Header
struct buddy_block {
    struct buddy_block* next;
    uint32_t level;
    uint32_t is_free;
    uint64_t magic;
};

#define BLOCK_MAGIC 0xB0DD1C0FFEULL

// Free lists by level
static struct buddy_block* free_lists[BUDDY_MAX_LEVELS];

// Main heap state
static void* heap_start;
static size_t heap_size;
static size_t bytes_allocated;

// Secure region (hidden from normal alloc)
static void* secure_start;
static size_t secure_size;
static size_t secure_used;

// Global exports
uint64_t g_total_memory = 0;
uint64_t g_heap_base = 0;
uint64_t g_heap_size = 0;
uint64_t g_secure_base = 0;

static inline size_t level_to_size(uint32_t level) {
    return BUDDY_MIN_SIZE << level;
}

static uint32_t size_to_level(size_t size) {
    size += sizeof(struct buddy_block);
    uint32_t level = 0;
    size_t block_size = BUDDY_MIN_SIZE;
    while (block_size < size && level < BUDDY_MAX_LEVELS - 1) {
        block_size <<= 1;
        level++;
    }
    return level;
}

static void* get_buddy(void* block, uint32_t level) {
    size_t block_size = level_to_size(level);
    uint64_t offset = (uint64_t)block - (uint64_t)heap_start;
    uint64_t buddy_offset = offset ^ block_size;
    return (void*)((uint64_t)heap_start + buddy_offset);
}

static void split_block(struct buddy_block* block, uint32_t level) {
    if (level == 0) return;
    size_t half = level_to_size(level - 1);
    
    struct buddy_block* buddy = (struct buddy_block*)((uint64_t)block + half);
    buddy->level = level - 1;
    buddy->is_free = 1;
    buddy->magic = BLOCK_MAGIC;
    
    block->level = level - 1;
    
    buddy->next = free_lists[level - 1];
    free_lists[level - 1] = buddy;
    block->next = free_lists[level - 1];
    free_lists[level - 1] = block;
}

// Initialize using E820 memory map
void buddy_init_e820(struct e820_entry* entries, int count, uint64_t* out_secure_base) {
    vga_puts("DEBUG: buddy_init_e820\n");
    
    // Find largest usable region above 1MB
    uint64_t best_base = 0;
    uint64_t best_size = 0;
    
    for (int i = 0; i < count; i++) {
        if (entries[i].type == E820_TYPE_USABLE) {
            // Skip regions below 1MB
            uint64_t base = entries[i].base;
            uint64_t len = entries[i].length;
            
            if (base < 0x100000) {
                if (base + len > 0x100000) {
                    len = (base + len) - 0x100000;
                    base = 0x100000;
                } else {
                    continue;
                }
            }
            
            // Skip kernel area (1MB to ~2MB)
            if (base < 0x200000) {
                if (base + len > 0x200000) {
                    len = (base + len) - 0x200000;
                    base = 0x200000;
                } else {
                    continue;
                }
            }
            
            if (len > best_size) {
                best_base = base;
                best_size = len;
            }
        }
    }
    
    if (best_size < 0x80000) { // Minimum 512KB
        vga_puts("WARN: E820 failed, using default\n");
        best_base = 0x200000;
        best_size = 0x100000; // 1MB fallback
    }
    
    // Reserve top 64KB for secure storage
    if (best_size > SECURE_REGION_SIZE * 2) {
        secure_size = SECURE_REGION_SIZE;
        secure_start = (void*)(best_base + best_size - secure_size);
        secure_used = 0;
        best_size -= secure_size;
        
        if (out_secure_base) {
            *out_secure_base = (uint64_t)secure_start;
        }
        g_secure_base = (uint64_t)secure_start;
    }
    
    // Initialize main heap
    buddy_init((void*)best_base, best_size);
    
    g_heap_base = best_base;
    g_heap_size = best_size;
}

void buddy_init(void* start, size_t size) {
    vga_puts("DEBUG: buddy_init start\n");
    heap_start = start;
    heap_size = size;
    bytes_allocated = 0;
    
    for (int i = 0; i < BUDDY_MAX_LEVELS; i++) {
        free_lists[i] = NULL;
    }
    
    // Find max level that fits
    uint32_t max_level = 0;
    size_t block_size = BUDDY_MIN_SIZE;
    while ((block_size << 1) <= size && max_level < BUDDY_MAX_LEVELS - 1) {
        block_size <<= 1;
        max_level++;
    }
    
    // Create initial block
    struct buddy_block* initial = (struct buddy_block*)start;
    initial->level = max_level;
    initial->is_free = 1;
    initial->magic = BLOCK_MAGIC;
    initial->next = NULL;
    
    free_lists[max_level] = initial;
}

void* buddy_alloc(size_t size) {
    if (size == 0) return NULL;
    
    uint32_t needed = size_to_level(size);
    
    // Find smallest available block
    uint32_t level = needed;
    while (level < BUDDY_MAX_LEVELS && !free_lists[level]) {
        level++;
    }
    
    if (level >= BUDDY_MAX_LEVELS) return NULL;
    
    // Split if necessary
    while (level > needed) {
        struct buddy_block* block = free_lists[level];
        free_lists[level] = block->next;
        split_block(block, level);
        level--;
    }
    
    // Remove from free list
    struct buddy_block* block = free_lists[needed];
    if (!block) return NULL;
    
    free_lists[needed] = block->next;
    block->is_free = 0;
    bytes_allocated += level_to_size(needed);
    
    return (void*)((uint64_t)block + sizeof(struct buddy_block));
}

void buddy_free(void* ptr) {
    if (!ptr) return;
    
    struct buddy_block* block = (struct buddy_block*)((uint64_t)ptr - sizeof(struct buddy_block));
    
    if (block->magic != BLOCK_MAGIC) {
        vga_puts("WARN: Invalid free\n");
        return;
    }
    
    block->is_free = 1;
    bytes_allocated -= level_to_size(block->level);
    
    // Try to coalesce with buddy
    while (block->level < BUDDY_MAX_LEVELS - 1) {
        struct buddy_block* buddy = get_buddy(block, block->level);
        
        if ((uint64_t)buddy < (uint64_t)heap_start ||
            (uint64_t)buddy >= (uint64_t)heap_start + heap_size) {
            break;
        }
        
        if (!buddy->is_free || buddy->level != block->level) {
            break;
        }
        
        // Remove buddy from free list
        struct buddy_block** pp = &free_lists[block->level];
        while (*pp && *pp != buddy) {
            pp = &(*pp)->next;
        }
        if (*pp) *pp = buddy->next;
        
        // Merge
        if (buddy < block) block = buddy;
        block->level++;
    }
    
    block->next = free_lists[block->level];
    free_lists[block->level] = block;
}

void buddy_stats(size_t* total, size_t* used, size_t* free) {
    if (total) *total = heap_size;
    if (used) *used = bytes_allocated;
    if (free) *free = heap_size - bytes_allocated;
}

// Secure Region (simple bump allocator)
void secure_region_init(void* base, size_t size) {
    secure_start = base;
    secure_size = size;
    secure_used = 0;
}

void* secure_alloc(size_t size) {
    if (!secure_start || secure_used + size > secure_size) {
        return NULL;
    }
    void* ptr = (void*)((uint64_t)secure_start + secure_used);
    secure_used += (size + 15) & ~15; // 16-byte align
    return ptr;
}

void secure_free(void* ptr) {
    (void)ptr; // Bump allocator doesn't support free
}