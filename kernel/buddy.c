// buddy.c - Implementation
#include "buddy.h"
#include "libc.h"
#include "vga.h"

// Buddy block metadata
struct buddy_block {
    struct buddy_block* next;
    uint32_t level;         // Block level (0 = 4KB, 1 = 8KB, etc.)
    uint32_t is_free;       // 1 = free, 0 = allocated
    uint64_t magic;         // Magic number for validation
};

#define BLOCK_MAGIC 0xB0DD1C0FFEULL

// Free lists for each level
static struct buddy_block* free_lists[BUDDY_MAX_LEVELS];

// Heap metadata
static void* heap_start;
static size_t heap_size;
static size_t bytes_allocated;

// Get block size for level
static inline size_t level_to_size(uint32_t level) {
    return BUDDY_MIN_SIZE << level;
}

// Get level for requested size
static uint32_t size_to_level(size_t size) {
    size += sizeof(struct buddy_block); // Add metadata overhead
    
    uint32_t level = 0;
    size_t block_size = BUDDY_MIN_SIZE;
    
    while (block_size < size && level < BUDDY_MAX_LEVELS - 1) {
        block_size <<= 1;
        level++;
    }
    
    return level;
}

// Get buddy address for a block
static void* get_buddy(void* block, uint32_t level) {
    size_t block_size = level_to_size(level);
    uint64_t offset = (uint64_t)block - (uint64_t)heap_start;
    uint64_t buddy_offset = offset ^ block_size;
    return (void*)((uint64_t)heap_start + buddy_offset);
}

// Split a block into two buddies
static void split_block(struct buddy_block* block, uint32_t level) {
    if (level == 0) return;
    
    size_t half_size = level_to_size(level - 1);
    
    // Create buddy block
    struct buddy_block* buddy = (struct buddy_block*)((uint64_t)block + half_size);
    buddy->level = level - 1;
    buddy->is_free = 1;
    buddy->magic = BLOCK_MAGIC;
    
    // Update original block
    block->level = level - 1;
    
    // Add both to free list
    buddy->next = free_lists[level - 1];
    free_lists[level - 1] = buddy;
    
    block->next = free_lists[level - 1];
    free_lists[level - 1] = block;
}

// Initialize buddy allocator
void buddy_init(void* start, size_t size) {
    vga_puts("DEBUG: buddy_init start\n");
    heap_start = start;
    heap_size = size;
    bytes_allocated = 0;
    
    // Clear free lists
    for (int i = 0; i < BUDDY_MAX_LEVELS; i++) {
        free_lists[i] = NULL;
    }
    

    // Find maximum level that fits in heap
    uint32_t max_level = 0;
    size_t max_block_size = BUDDY_MIN_SIZE;
    
    // Safety check: ensure we don't exceed array bounds
    while ((max_block_size << 1) <= size && max_level < BUDDY_MAX_LEVELS - 1) {
        max_block_size <<= 1;
        max_level++;
    }
    
    // Create initial large block
    struct buddy_block* initial = (struct buddy_block*)start;
    initial->level = max_level;
    initial->is_free = 1;
    initial->magic = BLOCK_MAGIC;
    initial->next = NULL;
    
    free_lists[max_level] = initial;
}

// Allocate memory
void* buddy_alloc(size_t size) {
    if (size == 0 || size > heap_size) {
        return NULL;
    }
    
    uint32_t level = size_to_level(size);
    
    // Find free block at requested level or higher
    uint32_t search_level = level;
    while (search_level < BUDDY_MAX_LEVELS && !free_lists[search_level]) {
        search_level++;
    }
    
    if (search_level >= BUDDY_MAX_LEVELS) {
        return NULL; // Out of memory
    }
    
    // Get block from free list
    struct buddy_block* block = free_lists[search_level];
    free_lists[search_level] = block->next;
    
    // Split block down to requested level
    while (search_level > level) {
        split_block(block, search_level);
        search_level--;
        
        // Re-get block from lower level
        block = free_lists[search_level];
        free_lists[search_level] = block->next;
    }
    
    // Mark as allocated
    block->is_free = 0;
    block->next = NULL;
    
    bytes_allocated += level_to_size(level);
    
    // Return pointer after metadata
    return (void*)((uint64_t)block + sizeof(struct buddy_block));
}

// Free memory
void buddy_free(void* ptr) {
    if (!ptr) return;
    
    // Get block metadata
    struct buddy_block* block = (struct buddy_block*)((uint64_t)ptr - sizeof(struct buddy_block));
    
    // Validation
    if (block->magic != BLOCK_MAGIC) {
        return; // Invalid block
    }
    
    if (block->is_free) {
        return; // Double free
    }
    
    uint32_t level = block->level;
    bytes_allocated -= level_to_size(level);
    
    // Try to coalesce with buddy
    while (level < BUDDY_MAX_LEVELS - 1) {
        void* buddy_addr = get_buddy(block, level);
        
        // Check if buddy is valid and free
        if ((uint64_t)buddy_addr < (uint64_t)heap_start ||
            (uint64_t)buddy_addr >= (uint64_t)heap_start + heap_size) {
            break;
        }
        
        struct buddy_block* buddy = (struct buddy_block*)buddy_addr;
        
        if (buddy->magic != BLOCK_MAGIC || !buddy->is_free || buddy->level != level) {
            break;
        }
        
        // Remove buddy from free list
        struct buddy_block** ptr = &free_lists[level];
        while (*ptr && *ptr != buddy) {
            ptr = &(*ptr)->next;
        }
        
        if (*ptr) {
            *ptr = buddy->next;
        }
        
        // Coalesce
        if (buddy < block) {
            block = buddy;
        }
        
        level++;
        block->level = level;
    }
    
    // Add to free list
    block->is_free = 1;
    block->next = free_lists[level];
    free_lists[level] = block;
}

// Get statistics
void buddy_stats(size_t* total, size_t* used, size_t* free_bytes) {
    *total = heap_size;
    *used = bytes_allocated;
    *free_bytes = heap_size - bytes_allocated;
}