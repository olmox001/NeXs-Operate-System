/*
 * sblock.c - Signed Memory Blocks Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "sblock.h"
#include "buddy.h"
#include "libc.h"
#include "process.h"

// Simple CRC32 for signature
static uint32_t crc32(const void* data, size_t len) {
    const uint8_t* p = data;
    uint32_t crc = 0xFFFFFFFF;
    
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

struct sblock* sblock_alloc(size_t size, uint8_t owner_uid, uint8_t perms) {
    if (size == 0 || size > 1024 * 1024) return NULL; // 1MB max
    
    size_t total = sizeof(struct sblock) + size;
    struct sblock* blk = buddy_alloc(total);
    if (!blk) return NULL;
    
    memset(blk, 0, total);
    blk->magic = SBLOCK_MAGIC;
    blk->size = size;
    blk->owner_uid = owner_uid;
    blk->permissions = perms;
    blk->flags = SBLOCK_VALID;
    blk->ref_count = 1;
    blk->signature = 0;
    
    return blk;
}

void sblock_free(struct sblock* blk) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return;
    
    if (blk->ref_count > 0) blk->ref_count--;
    
    if (blk->ref_count == 0) {
        blk->magic = 0;  // Invalidate
        buddy_free(blk);
    }
}

int sblock_share(struct sblock* blk, uint8_t target_uid) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return -1;
    if (!(blk->permissions & SBLOCK_SHARE)) return -1;
    
    // Kernel blocks can only be shared with root
    if ((blk->flags & SBLOCK_KERNEL) && target_uid > UID_ROOT) {
        return -1;
    }
    
    // Increment ref count (max 255)
    if (blk->ref_count < 255) {
        blk->ref_count++;
        return 0;
    }
    return -1;
}

bool sblock_verify(struct sblock* blk) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return false;
    if (!(blk->flags & SBLOCK_VALID)) return false;
    
    uint32_t computed = crc32(blk->data, blk->size);
    return computed == blk->signature;
}

void sblock_sign(struct sblock* blk) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return;
    blk->signature = crc32(blk->data, blk->size);
}

void* sblock_access(struct sblock* blk, uint8_t uid, uint8_t perm) {
    if (!blk || blk->magic != SBLOCK_MAGIC) return NULL;
    if (!(blk->flags & SBLOCK_VALID)) return NULL;
    
    // Owner has full access
    if (uid == blk->owner_uid) {
        return blk->data;
    }
    
    // Kernel bypass
    if (uid == UID_KERNEL) {
        return blk->data;
    }
    
    // Check permission
    if (!(blk->permissions & perm)) {
        return NULL;
    }
    
    // Kernel blocks need root
    if ((blk->flags & SBLOCK_KERNEL) && uid > UID_ROOT) {
        return NULL;
    }
    
    return blk->data;
}
