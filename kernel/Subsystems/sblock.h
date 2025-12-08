/*
 * sblock.h - Signed Memory Blocks (Secure Zero-Copy IPC)
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * SBLOCK (Signed Block) is a mechanism for sharing memory between tasks
 * with cryptographic integrity verification (CRC32) and detailed
 * permission control (Read/Write/Exec/Share).
 */

#ifndef SBLOCK_H
#define SBLOCK_H

#include "kernel.h"

// =============================================================================
// Settings & Constants
// =============================================================================

// Signature Magic used to identify valid SBlocks
#define SBLOCK_MAGIC    0x53424C4B5349474Eull  // ASCII bytes for "SBLKSIGN"

// Permissions (Bitmask)
#define SBLOCK_READ     0x01    // Read Access
#define SBLOCK_WRITE    0x02    // Write Access
#define SBLOCK_EXEC     0x04    // Execution Access (JIT buffers)
#define SBLOCK_SHARE    0x08    // Can be re-shared

// Status Flags
#define SBLOCK_VALID    0x01    // Block is initialized and valid
#define SBLOCK_LOCKED   0x02    // Locked for modification (during Write)
#define SBLOCK_KERNEL   0x04    // Created by Kernel (Root only)

// =============================================================================
// Helper Structures
// =============================================================================

/**
 * Signed Block Header
 * Metadata prepended to the user data buffer.
 * Total Header Size: 32 bytes (Aligned)
 */
struct sblock {
    uint64_t    magic;          // 0x00: Validation Magic
    uint32_t    signature;      // 0x08: CRC32 of Data Payload
    uint32_t    size;           // 0x0C: Payload Size in Bytes
    
    uint8_t     owner_uid;      // 0x10: Creator User ID
    uint8_t     permissions;    // 0x11: Permission Bits
    uint8_t     flags;          // 0x12: Status Bits
    uint8_t     ref_count;      // 0x13: Reference Counter (for shared GC)
    
    uint32_t    reserved;       // 0x14: Alignment Padding
    
    uint8_t     data[];         // 0x18: Flexible Array (Start of Payload)
};

// =============================================================================
// Public API
// =============================================================================

/**
 * Allocate a new signed block.
 * 
 * @param size Size of payload in bytes
 * @param owner_uid Creator ID (usually current_task->uid)
 * @param perms Initial permissions
 * @return Pointer to sblock header, or NULL on error
 */
struct sblock* sblock_alloc(size_t size, uint8_t owner_uid, uint8_t perms);

/**
 * Free a signed block.
 * Decrements reference count. If count reaches 0, memory is freed.
 * 
 * @param blk Block pointer
 */
void sblock_free(struct sblock* blk);

/**
 * Share a block with another task.
 * Currently just increments ref count (Conceptual Sharing).
 * In a full VM system, this would map pages.
 * 
 * @param blk Block to share
 * @param target_uid Target UID (Checked against restrictions)
 * @return 0 on success, -1 on denial
 */
int sblock_share(struct sblock* blk, uint8_t target_uid);

/**
 * Verify Integrity.
 * Recomputes CRC32 of payload and compares with header signature.
 * 
 * @return true if matches, false if corrupted/tampered
 */
bool sblock_verify(struct sblock* blk);

/**
 * Sign the block.
 * Computes CRC32 of valid payload and writes to header.
 * Must be called after modification.
 */
void sblock_sign(struct sblock* blk);

/**
 * Access Data Pointer.
 * Performs permission checks before returning pointer.
 * 
 * @param blk Block
 * @param uid Accessor UID
 * @param perm Requested Permission (READ/WRITE/EXEC)
 * @return Pointer to payload or NULL if denied
 */
void* sblock_access(struct sblock* blk, uint8_t uid, uint8_t perm);

#endif // SBLOCK_H
