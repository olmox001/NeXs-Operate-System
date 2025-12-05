/*
 * permissions.h - Capability-Based Security System Definitions
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

#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include "kernel.h"

// =============================================================================
// Capability Bitmasks
// =============================================================================
#define PERM_NONE           0x0000
#define PERM_MEMORY_ALLOC   0x0001  // Dynamic allocation
#define PERM_MEMORY_FREE    0x0002  // Dynamic deallocation
#define PERM_IO_READ        0x0004  // Protected I/O read access
#define PERM_IO_WRITE       0x0008  // Protected I/O write access
#define PERM_MSG_SEND       0x0010  // IPC Send
#define PERM_MSG_RECEIVE    0x0020  // IPC Receive
#define PERM_IRQ_INSTALL    0x0040  // Driver installation
#define PERM_IRQ_REMOVE     0x0080  // Driver removal
#define PERM_TASK_CREATE    0x0100  // Fork/Spawning
#define PERM_TASK_DESTROY   0x0200  // Process termination
#define PERM_PERM_GRANT     0x0400  // Administrative: Grant Rights
#define PERM_PERM_REVOKE    0x0800  // Administrative: Revoke Rights
#define PERM_KERNEL_MODE    0x1000  // Root Access (Bypasses checks)
#define PERM_SHELL_ACCESS   0x2000  // Interactive Terminal Access
#define PERM_DEBUG          0x4000  // Debugger APIs
#define PERM_ADMIN          0x8000  // Reserved / User Group Admin

// =============================================================================
// Security Structures
// =============================================================================
struct task_perms {
    uint32_t task_id;           // Owner ID
    uint16_t capabilities;      // Bitmap of Held Rights
    uint32_t parent_id;         // Creator ID
    uint64_t granted_time;      // Audit Timestamp
    bool active;                // Slot Status
};

// =============================================================================
// Public API
// =============================================================================

// Initialize Security Subsystem
void perm_init(void);

// Register Process
int perm_create_task(uint32_t task_id, uint32_t parent_id, uint16_t initial_perms);

// Terminate Process
void perm_destroy_task(uint32_t task_id);

// Modify Capabilities
int perm_grant(uint32_t granter_id, uint32_t target_id, uint16_t perms);
int perm_revoke(uint32_t revoker_id, uint32_t target_id, uint16_t perms);

// Validate Access
bool perm_check(uint32_t task_id, uint16_t perm);

// Inspection
uint16_t perm_get(uint32_t task_id);
const char* perm_name(uint16_t perm);

// Internal Logic
void perm_inherit(uint32_t child_id, uint32_t parent_id);

#endif // PERMISSIONS_H