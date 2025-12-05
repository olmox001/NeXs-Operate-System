/*
 * buddy.c - Buddy Memory Allocator Implementation
 *
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, NeXs Operate System
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "buddy.h"
#include "libc.h"
#include "vga.h"

// Buddy Block Metadata Header
struct buddy_block {
    struct buddy_block* next;
    uint32_t level;         // Block order (0=Min, 1=2xMin, etc.)
    uint32_t is_free;       // 1 = Free, 0 = Allocated
    uint64_t magic;         // Validation Cookie
};

// Magic Security Cookie (Coffee L33t Speak)
#define BLOCK_MAGIC 0xB0DD1C0FFEULL

// Array of free lists, indexed by level
static struct buddy_block* free_lists[BUDDY_MAX_LEVELS];

// Global Heap State
static void* heap_start;
static size_t heap_size;
static size_t bytes_allocated;

/**
 * Calculate block size for a given level
 */
static inline size_t level_to_size(uint32_t level) {
    return BUDDY_MIN_SIZE << level;
}

/**
 * Determine minimum level required for a given size
 */
static uint32_t size_to_level(size_t size) {
    size += sizeof(struct buddy_block); // Include metadata overhead
    
    uint32_t level = 0;
    size_t block_size = BUDDY_MIN_SIZE;
    
    while (block_size < size && level < BUDDY_MAX_LEVELS - 1) {
        block_size <<= 1;
        level++;
    }
    
    return level;
}

/**
 * Calculate address of the buddy block (XOR based)
 */
static void* get_buddy(void* block, uint32_t level) {
    size_t block_size = level_to_size(level);
    uint64_t offset = (uint64_t)block - (uint64_t)heap_start;
    uint64_t buddy_offset = offset ^ block_size;
    return (void*)((uint64_t)heap_start + buddy_offset);
}

/**
 * Split a free block into two smaller buddies
 */
static void split_block(struct buddy_block* block, uint32_t level) {
    if (level == 0) return;
    
    size_t half_size = level_to_size(level - 1);
    
    // Create new buddy block at half-size offset
    struct buddy_block* buddy = (struct buddy_block*)((uint64_t)block + half_size);
    buddy->level = level - 1;
    buddy->is_free = 1;
    buddy->magic = BLOCK_MAGIC;
    
    // Downgrade original block
    block->level = level - 1;
    
    // Link both into the lower-level free list
    buddy->next = free_lists[level - 1];
    free_lists[level - 1] = buddy;
    
    block->next = free_lists[level - 1];
    free_lists[level - 1] = block;
}

/**
 * Initialize the Buddy Allocator
 */
void buddy_init(void* start, size_t size) {
    vga_puts("DEBUG: buddy_init start\n");
    heap_start = start;
    heap_size = size;
    bytes_allocated = 0;
    
    // Reset free lists
    for (int i = 0; i < BUDDY_MAX_LEVELS; i++) {
        free_lists[i] = NULL;
    }
    
    // Calculate initial max block level fitting in heap
    uint32_t max_level = 0;
    size_t max_block_size = BUDDY_MIN_SIZE;
    
    // Ensure we don't exceed array bounds or heap limits
    while ((max_block_size << 1) <= size && max_level < BUDDY_MAX_LEVELS - 1) {
        max_block_size <<= 1;
        max_level++;
    }
    
    // Create the initial large root block
    struct buddy_block* initial = (struct buddy_block*)start;
    initial->level = max_level;
    initial->is_free = 1;
    initial->magic = BLOCK_MAGIC;
    initial->next = NULL;
    
    free_lists[max_level] = initial;
}

/**
 * Allocate memory
 */
void* buddy_alloc(size_t size) {
    if (size == 0 || size > heap_size) {
        return NULL;
    }
    
    uint32_t level = size_to_level(size);
    
    // Find smallest available free block
    uint32_t search_level = level;
    while (search_level < BUDDY_MAX_LEVELS && !free_lists[search_level]) {
        search_level++;
    }
    
    if (search_level >= BUDDY_MAX_LEVELS) {
        return NULL; // Out of memory
    }
    
    // Unlink block from free list
    struct buddy_block* block = free_lists[search_level];
    free_lists[search_level] = block->next;
    
    // Split downwards to the requested level
    while (search_level > level) {
        split_block(block, search_level);
        search_level--;
        
        // Re-acquire block from lower list (it was put at head)
        block = free_lists[search_level];
        free_lists[search_level] = block->next;
    }
    
    // Mark as allocated
    block->is_free = 0;
    block->next = NULL;
    
    bytes_allocated += level_to_size(level);
    
    // Return payload address (skip metadata)
    return (void*)((uint64_t)block + sizeof(struct buddy_block));
}

/**
 * Free memory
 */
void buddy_free(void* ptr) {
    if (!ptr) return;
    
    // Recover metadata pointer
    struct buddy_block* block = (struct buddy_block*)((uint64_t)ptr - sizeof(struct buddy_block));
    
    // Security Checks
    if (block->magic != BLOCK_MAGIC) {
        return; // Validation failure (Corruption?)
    }
    if (block->is_free) {
        return; // Double Free
    }
    
    uint32_t level = block->level;
    bytes_allocated -= level_to_size(level);
    
    // Attempt Coalescing (Merging buddies)
    while (level < BUDDY_MAX_LEVELS - 1) {
        void* buddy_addr = get_buddy(block, level);
        
        // Check bounds
        if ((uint64_t)buddy_addr < (uint64_t)heap_start ||
            (uint64_t)buddy_addr >= (uint64_t)heap_start + heap_size) {
            break;
        }
        
        struct buddy_block* buddy = (struct buddy_block*)buddy_addr;
        
        // Validate Buddy
        if (buddy->magic != BLOCK_MAGIC || !buddy->is_free || buddy->level != level) {
            break; // Cannot merge
        }
        
        // Remove buddy from free list
        struct buddy_block** prev_ptr = &free_lists[level];
        while (*prev_ptr && *prev_ptr != buddy) {
            prev_ptr = &(*prev_ptr)->next;
        }
        
        if (*prev_ptr) {
            *prev_ptr = buddy->next;
        }
        
        // Merge logic: Block becomes the lower address of the pair
        if (buddy < block) {
            block = buddy;
        }
        
        level++;
        block->level = level;
    }
    
    // Mark as free and add to list
    block->is_free = 1;
    block->next = free_lists[level];
    free_lists[level] = block;
}

/**
 * Return Allocator Statistics
 */
void buddy_stats(size_t* total, size_t* used, size_t* free_bytes) {
    *total = heap_size;
    *used = bytes_allocated;
    *free_bytes = heap_size - bytes_allocated;
}