/*
 * sblock.h - Signed Memory Blocks (Zero-Copy IPC)
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Provides secure memory sharing between tasks with:
 * - Signature validation (integrity)
 * - Reference counting (zero-copy)
 * - Permission-based access control
 */

#ifndef SBLOCK_H
#define SBLOCK_H

#include "kernel.h"

// Block Permissions
#define SBLOCK_READ     0x01
#define SBLOCK_WRITE    0x02
#define SBLOCK_EXEC     0x04
#define SBLOCK_SHARE    0x08

// Block Flags
#define SBLOCK_VALID    0x01
#define SBLOCK_LOCKED   0x02
#define SBLOCK_KERNEL   0x04

// Signature Magic
#define SBLOCK_MAGIC    0x53424C4B5349474Eull  // "SBLKSIGN"

// =============================================================================
// Signed Block Structure (32 bytes header + data)
// =============================================================================
struct sblock {
    uint64_t    magic;          // SBLOCK_MAGIC
    uint32_t    signature;      // CRC32 of data
    uint32_t    size;           // Data size in bytes
    
    uint8_t     owner_uid;      // Creator UID
    uint8_t     permissions;    // R/W/X/Share
    uint8_t     flags;          // Valid/Locked/Kernel
    uint8_t     ref_count;      // Reference counter
    
    uint32_t    reserved;       // Alignment padding
    
    uint8_t     data[];         // Flexible array member
};

// =============================================================================
// API Functions
// =============================================================================

/**
 * Allocate a signed block
 * @param size Data size
 * @param owner_uid Owner's UID
 * @param perms Initial permissions
 * @return Pointer to block or NULL on failure
 */
struct sblock* sblock_alloc(size_t size, uint8_t owner_uid, uint8_t perms);

/**
 * Free a signed block (decrements ref count)
 * @param blk Block pointer
 */
void sblock_free(struct sblock* blk);

/**
 * Share a block with another task (zero-copy)
 * @param blk Block to share
 * @param target_uid Target task UID
 * @return 0 on success, -1 on permission error
 */
int sblock_share(struct sblock* blk, uint8_t target_uid);

/**
 * Verify block signature
 * @param blk Block to verify
 * @return true if valid, false if corrupted
 */
bool sblock_verify(struct sblock* blk);

/**
 * Update block signature after modification
 * @param blk Block to sign
 */
void sblock_sign(struct sblock* blk);

/**
 * Get block data pointer (with permission check)
 * @param blk Block
 * @param uid Requesting UID
 * @param perm Required permission (SBLOCK_READ/WRITE/EXEC)
 * @return Data pointer or NULL on permission denied
 */
void* sblock_access(struct sblock* blk, uint8_t uid, uint8_t perm);

#endif // SBLOCK_H
