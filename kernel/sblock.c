/*
 * sblock.c - Signed Memory Blocks Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "sblock.h"
#include "buddy.h"
#include "libx.h"
#include "process.h"

// =============================================================================
// Crypto Utilities
// =============================================================================

/**
 * CRC32 Implementation (Standard IEEE 802.3)
 * Used for light-weight integrity hashing.
 */
static uint32_t crc32(const void* data, size_t len) {
    const uint8_t* p = data;
    uint32_t crc = 0xFFFFFFFF;
    
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            // Polynomial 0xEDB88320
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

// =============================================================================
// API Implementation
// =============================================================================

/**
 * Allocate SBlock
 */
struct sblock* sblock_alloc(size_t size, uint8_t owner_uid, uint8_t perms) {
    // Limit Max Block Size to avoid massive allocations (1MB limit)
    if (size == 0 || size > 1024 * 1024) return NULL;
    
    // Allocate Header + Payload
    size_t total = sizeof(struct sblock) + size;
    struct sblock* blk = buddy_alloc(total);
    if (!blk) return NULL;
    
    // Initialize Header
    memset(blk, 0, total);
    blk->magic = SBLOCK_MAGIC;
    blk->size = size;
    blk->owner_uid = owner_uid;
    blk->permissions = perms;
    blk->flags = SBLOCK_VALID;
    blk->ref_count = 1; // Start with 1 reference (creator)
    blk->signature = 0; // Invalid unti sblock_sign is called
    
    return blk;
}

/**
 * Free SBlock
 */
void sblock_free(struct sblock* blk) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return;
    
    // Decrement Reference Count
    if (blk->ref_count > 0) blk->ref_count--;
    
    // Physical Free only if no references remain
    if (blk->ref_count == 0) {
        blk->magic = 0;  // Prevent use-after-free accidents
        buddy_free(blk);
    }
}

/**
 * Share SBlock
 * Increments reference count to prevent premature freeing by owner.
 */
int sblock_share(struct sblock* blk, uint8_t target_uid) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return -1;
    
    // Check Share Permission
    if (!(blk->permissions & SBLOCK_SHARE)) return -1;
    
    // Security Restriction: Kernel Blocks (UID 0) cannot be shared with non-roots
    // unless explicitly allowed (TODO: finer grain control)
    if ((blk->flags & SBLOCK_KERNEL) && target_uid > UID_ROOT) {
        return -1;
    }
    
    // Prevent Overflow of ref_count
    if (blk->ref_count < 255) {
        blk->ref_count++;
        return 0;
    }
    return -1; // Too many shares
}

/**
 * Verify Integrity
 */
bool sblock_verify(struct sblock* blk) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return false;
    if (!(blk->flags & SBLOCK_VALID)) return false;
    
    // Recompute CRC and compare
    uint32_t computed = crc32(blk->data, blk->size);
    return computed == blk->signature;
}

/**
 * Sign Block
 * Must be called after writing data to update the signature.
 */
void sblock_sign(struct sblock* blk) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return;
    
    // Compute CRC and store
    blk->signature = crc32(blk->data, blk->size);
}

/**
 * Accessor with Security Checks
 */
void* sblock_access(struct sblock* blk, uint8_t uid, uint8_t perm) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return NULL;
    if (!(blk->flags & SBLOCK_VALID)) return NULL;
    
    // 1. Owner always has access
    if (uid == blk->owner_uid) {
        return blk->data;
    }
    
    // 2. Kernel/Root always has access
    if (uid == UID_KERNEL) {
        return blk->data;
    }
    
    // 3. Permission Check
    if (!(blk->permissions & perm)) {
        return NULL; // Access Denied
    }
    
    // 4. Kernel Block Protection
    // Even with permission bits, non-root users cannot touch Kernel Blocks
    if ((blk->flags & SBLOCK_KERNEL) && uid > UID_ROOT) {
        return NULL;
    }
    
    return blk->data;
}
