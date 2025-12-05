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
#include "libc.h"
#include "buddy.h"

// Global Permission Table (Static Allocation for Reliability)
static struct task_perms task_perms_table[MAX_TASKS];
static uint64_t perm_timestamp = 0;

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

/**
 * Initialize Capability System
 * Sets up Kernel (Task 0) with full privileges.
 */
void perm_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_perms_table[i].task_id = i;
        task_perms_table[i].capabilities = PERM_NONE;
        task_perms_table[i].parent_id = 0;
        task_perms_table[i].granted_time = 0;
        task_perms_table[i].active = false;
    }
    
    // Grant Omnipotent Permissions to Kernel Task (ID 0)
    task_perms_table[0].capabilities = 0xFFFF;
    task_perms_table[0].parent_id = 0;
    task_perms_table[0].granted_time = 0;
    task_perms_table[0].active = true;
    
    perm_timestamp = 0;
}

/**
 * Register a new Task with limited permissions
 */
int perm_create_task(uint32_t task_id, uint32_t parent_id, uint16_t initial_perms) {
    if (task_id >= MAX_TASKS || parent_id >= MAX_TASKS) {
        PANIC("Invalid Task ID in Perm System");
        return -1;
    }
    
    // Security Check: Does parent have right to create tasks?
    if (!perm_check(parent_id, PERM_TASK_CREATE)) {
        return -1; // Access Denied
    }
    
    // Slot Availability Check
    if (task_perms_table[task_id].active) {
        return -1; // Collision
    }
    
    perm_timestamp++;
    
    // Initialize Task Entry
    task_perms_table[task_id].task_id = task_id;
    task_perms_table[task_id].capabilities = initial_perms;
    task_perms_table[task_id].parent_id = parent_id;
    task_perms_table[task_id].granted_time = perm_timestamp;
    task_perms_table[task_id].active = true;
    
    // Inheritance Logic
    perm_inherit(task_id, parent_id);
    
    return 0;
}

/**
 * Deregister Task
 */
void perm_destroy_task(uint32_t task_id) {
    if (task_id >= MAX_TASKS || task_id == 0) {
        return; // Protection Violation (Cannot kill Kernel)
    }
    
    task_perms_table[task_id].active = false;
    task_perms_table[task_id].capabilities = PERM_NONE;
}

/**
 * Grant Privilege to Task
 */
int perm_grant(uint32_t granter_id, uint32_t target_id, uint16_t perms) {
    if (granter_id >= MAX_TASKS || target_id >= MAX_TASKS) {
        return -1;
    }
    
    // Security Check: Does granter have authority?
    if (!perm_check(granter_id, PERM_PERM_GRANT)) {
        return -1; // Access Denied
    }
    
    // Target Existence Check
    if (!task_perms_table[target_id].active) {
        return -1;
    }
    
    // Apply Flags
    task_perms_table[target_id].capabilities |= perms;
    task_perms_table[target_id].granted_time = ++perm_timestamp;
    
    return 0;
}

/**
 * Revoke Privilege from Task
 */
int perm_revoke(uint32_t revoker_id, uint32_t target_id, uint16_t perms) {
    if (revoker_id >= MAX_TASKS || target_id >= MAX_TASKS) {
        return -1;
    }
    
    // Security Check
    if (!perm_check(revoker_id, PERM_PERM_REVOKE)) {
        return -1;
    }
    
    // Kernel Integrity Protection
    if (target_id == 0) {
        return -1; // Cannot revoke capabilities from Kernel
    }
    
    if (!task_perms_table[target_id].active) {
        return -1;
    }
    
    // Remove Flags (Bitwise Clear)
    task_perms_table[target_id].capabilities &= ~perms;
    task_perms_table[target_id].granted_time = ++perm_timestamp;
    
    return 0;
}

/**
 * Verify Permission
 */
bool perm_check(uint32_t task_id, uint16_t perm) {
    if (task_id >= MAX_TASKS) {
        return false;
    }
    
    if (!task_perms_table[task_id].active) {
        return false;
    }
    
    // Root / Kernel Mode Bypass
    if (task_perms_table[task_id].capabilities & PERM_KERNEL_MODE) {
        return true;
    }
    
    // Standard Flag Check
    return (task_perms_table[task_id].capabilities & perm) == perm;
}

/**
 * Retrieve Capability Mask
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
    inheritable &= ~(PERM_PERM_GRANT | PERM_PERM_REVOKE | PERM_KERNEL_MODE);
    
    // Merge with existing permissions
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