/*
 * permissions.c - Capability-Based Security System
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

#include "permissions.h"
#include "libx.h"
#include "buddy.h"

// =============================================================================
// Global Permission Table
// =============================================================================
// Static Allocation for Reliability (Avoids malloc dependencies in sec layer)
static struct task_perms task_perms_table[MAX_TASKS];
static uint64_t perm_timestamp = 0; // Audit counter

// Lock for SMP Safety
#include "spinlock.h"
static spinlock_t perm_lock = {0};

// Human-Readable Permission Names (Debug)
static const char* perm_names[] = {
    "MEMORY_ALLOC",
    "MEMORY_FREE",
    "IO_READ",
    "IO_WRITE",
    "MSG_SEND",
    "MSG_RECEIVE",
    "IRQ_INSTALL",
    "IRQ_REMOVE",
    "TASK_CREATE",
    "TASK_DESTROY",
    "PERM_GRANT",
    "PERM_REVOKE",
    "KERNEL_MODE",
    "SHELL_ACCESS",
    "DEBUG",
    "ADMIN"
};

// Internal prototype for lock management
// We include lock/unlock logic inside public functions directly
// or use helpers if code is complex. Simple acquire/release here.

/**
 * Initialize Capability System
 * Sets up Kernel (Task 0) with full privileges.
 */
void perm_init(void) {
    // Zero all entries
    for (int i = 0; i < MAX_TASKS; i++) {
        task_perms_table[i].task_id = i;
        task_perms_table[i].capabilities = PERM_NONE;
        task_perms_table[i].parent_id = 0;
        task_perms_table[i].granted_time = 0;
        task_perms_table[i].active = false;
    }
    
    // Grant Omnipotent Permissions to Kernel Task (ID 0)
    task_perms_table[0].capabilities = 0xFFFF; // All bits set
    task_perms_table[0].parent_id = 0;
    task_perms_table[0].granted_time = 0;
    task_perms_table[0].active = true;
    
    perm_timestamp = 0;
    spinlock_init(&perm_lock);
}

/**
 * Register a new Task with limited permissions
 * Called by task_create
 */
int perm_create_task(uint32_t task_id, uint32_t parent_id, uint16_t initial_perms) {
    if (task_id >= MAX_TASKS || parent_id >= MAX_TASKS) {
        PANIC("Invalid Task ID in Perm System");
        return -1;
    }
    
    int result = 0;
    
    spinlock_acquire(&perm_lock);
    
    // Re-verify bounds inside lock
    if (task_id >= MAX_TASKS || parent_id >= MAX_TASKS) {
        spinlock_release(&perm_lock);
        return -1;
    }

    // Security Check: Does parent have right to create tasks?
    // Check directly to avoid recursive lock
    if (!task_perms_table[parent_id].active || 
        (!(task_perms_table[parent_id].capabilities & PERM_TASK_CREATE) &&
         !(task_perms_table[parent_id].capabilities & PERM_KERNEL_MODE))) {
        result = -1;
    } 
    // Slot Availability Check
    else if (task_perms_table[task_id].active) {
        result = -1; // Collision
    }
    else {
        perm_timestamp++;
    
        // Initialize Task Entry
        task_perms_table[task_id].task_id = task_id;
        task_perms_table[task_id].capabilities = initial_perms;
        task_perms_table[task_id].parent_id = parent_id;
        task_perms_table[task_id].granted_time = perm_timestamp;
        task_perms_table[task_id].active = true;
    
        // Inheritance Logic: Apply constraints
        // Internal call, safe since we hold lock
        perm_inherit(task_id, parent_id);
    }
    
    spinlock_release(&perm_lock);
    return result;
}

/**
 * Deregister Task
 * Called by exit()
 */
void perm_destroy_task(uint32_t task_id) {
    if (task_id >= MAX_TASKS || task_id == 0) {
        return; // Protection Violation (Cannot kill Kernel slot 0)
    }
    
    // Deactivate entry
    task_perms_table[task_id].active = false;
    task_perms_table[task_id].capabilities = PERM_NONE;
}

/**
 * Grant Privilege to Task
 */
int perm_grant(uint32_t granter_id, uint32_t target_id, uint16_t perms) {
    spinlock_acquire(&perm_lock);
    
    // Bounds Check with lock held (safe)
    if (granter_id >= MAX_TASKS || target_id >= MAX_TASKS) {
        spinlock_release(&perm_lock);
        return -1;
    }
    
    // Security Check: Does granter have authority to grant permissions?
    // NOTE: perm_check calls spinlock internally? NO, perm_check should be lock-aware.
    // We cannot call public perm_check while holding lock (Self-Deadlock).
    // We need an internal version or just check array directly since we have the lock.
    
    // Check Granter Authority directly
    if (!task_perms_table[granter_id].active || 
        (!(task_perms_table[granter_id].capabilities & PERM_PERM_GRANT) &&
         !(task_perms_table[granter_id].capabilities & PERM_KERNEL_MODE))) { // Kernel mode override
        spinlock_release(&perm_lock);
        return -1; // Access Denied
    }
    
    // Target Existence Check
    if (!task_perms_table[target_id].active) {
        spinlock_release(&perm_lock);
        return -1;
    }
    
    // Apply Flags (Bitwise OR)
    task_perms_table[target_id].capabilities |= perms;
    task_perms_table[target_id].granted_time = ++perm_timestamp;
    
    spinlock_release(&perm_lock);
    return 0;
}

/**
 * Revoke Privilege from Task
 */
int perm_revoke(uint32_t revoker_id, uint32_t target_id, uint16_t perms) {
    spinlock_acquire(&perm_lock);
    
    if (revoker_id >= MAX_TASKS || target_id >= MAX_TASKS) {
        spinlock_release(&perm_lock);
        return -1;
    }
    
    // Security Check: Revoke Authority
    if (!task_perms_table[revoker_id].active ||
        (!(task_perms_table[revoker_id].capabilities & PERM_PERM_REVOKE) &&
         !(task_perms_table[revoker_id].capabilities & PERM_KERNEL_MODE))) {
        spinlock_release(&perm_lock);
        return -1;
    }
    
    // Kernel Integrity Protection
    if (target_id == 0) {
        spinlock_release(&perm_lock);
        return -1; // Cannot revoke capability from Kernel
    }
    
    if (!task_perms_table[target_id].active) {
        spinlock_release(&perm_lock);
        return -1;
    }
    
    // Remove Flags (Bitwise AND with Inverse)
    task_perms_table[target_id].capabilities &= ~perms;
    task_perms_table[target_id].granted_time = ++perm_timestamp;
    
    spinlock_release(&perm_lock);
    return 0;
}

/**
 * Verify Permission
 */
bool perm_check(uint32_t task_id, uint16_t perm) {
    bool result = false;
    spinlock_acquire(&perm_lock);
    
    if (task_id < MAX_TASKS && task_perms_table[task_id].active) {
        // Root / Kernel Mode Bypass
        if (task_perms_table[task_id].capabilities & PERM_KERNEL_MODE) {
            result = true;
        } else {
            // Standard Flag Check
            result = ((task_perms_table[task_id].capabilities & perm) == perm);
        }
    }
    
    spinlock_release(&perm_lock);
    return result;
}

/**
 * Retrieve Capability Mask for inspection
 */
uint16_t perm_get(uint32_t task_id) {
    if (task_id >= MAX_TASKS) {
        return PERM_NONE;
    }
    
    if (!task_perms_table[task_id].active) {
        return PERM_NONE;
    }
    
    return task_perms_table[task_id].capabilities;
}

/**
 * Inheritance Logic
 * Automatically called during task creation.
 */
void perm_inherit(uint32_t child_id, uint32_t parent_id) {
    if (child_id >= MAX_TASKS || parent_id >= MAX_TASKS) {
        return;
    }
    
    if (!task_perms_table[parent_id].active) {
        return;
    }
    
    // Principle of Least Privilege:
    // Child inherits most permissions from parent, BUT...
    // Critical administrative permissions are NOT inherited automatically.
    uint16_t inheritable = task_perms_table[parent_id].capabilities;
    
    // Filter out Admin rights
    inheritable &= ~(PERM_PERM_GRANT | PERM_PERM_REVOKE | PERM_KERNEL_MODE);
    
    // Merge with existing permissions (e.g. initial_perms passed to create)
    task_perms_table[child_id].capabilities |= inheritable;
}

/**
 * Debug: Get string name of permission bit
 */
const char* perm_name(uint16_t perm) {
    for (int i = 0; i < 16; i++) {
        if ((1 << i) == perm) {
            return perm_names[i];
        }
    }
    return "UNKNOWN";
}