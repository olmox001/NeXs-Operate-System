// permissions.c - Implementation
#include "permissions.h"
#include "libc.h"
#include "buddy.h"

// Task permission table
static struct task_perms task_perms_table[MAX_TASKS];
static uint64_t perm_timestamp = 0;

// Permission names for debugging
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

// Initialize permission system
void perm_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_perms_table[i].task_id = i;
        task_perms_table[i].capabilities = PERM_NONE;
        task_perms_table[i].parent_id = 0;
        task_perms_table[i].granted_time = 0;
        task_perms_table[i].active = false;
    }
    
    // Task 0 (kernel) has all permissions
    task_perms_table[0].capabilities = 0xFFFF;
    task_perms_table[0].parent_id = 0;
    task_perms_table[0].granted_time = 0;
    task_perms_table[0].active = true;
    
    perm_timestamp = 0;
}

// Create task with initial permissions
int perm_create_task(uint32_t task_id, uint32_t parent_id, uint16_t initial_perms) {
    if (task_id >= MAX_TASKS || parent_id >= MAX_TASKS) {
        PANIC("Invalid Task ID in Perm System");
        return -1;
    }
    
    // Check if parent has permission to create tasks
    if (!perm_check(parent_id, PERM_TASK_CREATE)) {
        return -1;
    }
    
    // Check if task slot is available
    if (task_perms_table[task_id].active) {
        return -1; // Task already exists
    }
    
    perm_timestamp++;
    
    // Create task entry
    task_perms_table[task_id].task_id = task_id;
    task_perms_table[task_id].capabilities = initial_perms;
    task_perms_table[task_id].parent_id = parent_id;
    task_perms_table[task_id].granted_time = perm_timestamp;
    task_perms_table[task_id].active = true;
    
    // Inherit permissions from parent (with restrictions)
    perm_inherit(task_id, parent_id);
    
    return 0;
}

// Destroy task
void perm_destroy_task(uint32_t task_id) {
    if (task_id >= MAX_TASKS || task_id == 0) {
        return; // Can't destroy kernel task
    }
    
    task_perms_table[task_id].active = false;
    task_perms_table[task_id].capabilities = PERM_NONE;
}

// Grant permission
int perm_grant(uint32_t granter_id, uint32_t target_id, uint16_t perms) {
    if (granter_id >= MAX_TASKS || target_id >= MAX_TASKS) {
        return -1;
    }
    
    // Check if granter has permission to grant
    if (!perm_check(granter_id, PERM_PERM_GRANT)) {
        return -1;
    }
    
    // Check if target task exists
    if (!task_perms_table[target_id].active) {
        return -1;
    }
    
    // Grant permissions (OR operation)
    task_perms_table[target_id].capabilities |= perms;
    task_perms_table[target_id].granted_time = ++perm_timestamp;
    
    return 0;
}

// Revoke permission
int perm_revoke(uint32_t revoker_id, uint32_t target_id, uint16_t perms) {
    if (revoker_id >= MAX_TASKS || target_id >= MAX_TASKS) {
        return -1;
    }
    
    // Check if revoker has permission to revoke
    if (!perm_check(revoker_id, PERM_PERM_REVOKE)) {
        return -1;
    }
    
    // Can't revoke from kernel task
    if (target_id == 0) {
        return -1;
    }
    
    // Check if target task exists
    if (!task_perms_table[target_id].active) {
        return -1;
    }
    
    // Revoke permissions (AND NOT operation)
    task_perms_table[target_id].capabilities &= ~perms;
    task_perms_table[target_id].granted_time = ++perm_timestamp;
    
    return 0;
}

// Check permission
bool perm_check(uint32_t task_id, uint16_t perm) {
    if (task_id >= MAX_TASKS) {
        return false;
    }
    
    if (!task_perms_table[task_id].active) {
        return false;
    }
    
    // Kernel mode has all permissions
    if (task_perms_table[task_id].capabilities & PERM_KERNEL_MODE) {
        return true;
    }
    
    return (task_perms_table[task_id].capabilities & perm) == perm;
}

// Get all permissions
uint16_t perm_get(uint32_t task_id) {
    if (task_id >= MAX_TASKS) {
        return PERM_NONE;
    }
    
    if (!task_perms_table[task_id].active) {
        return PERM_NONE;
    }
    
    return task_perms_table[task_id].capabilities;
}

// Inherit permissions from parent
void perm_inherit(uint32_t child_id, uint32_t parent_id) {
    if (child_id >= MAX_TASKS || parent_id >= MAX_TASKS) {
        return;
    }
    
    if (!task_perms_table[parent_id].active) {
        return;
    }
    
    // Child inherits subset of parent's permissions
    // Exclude dangerous permissions like PERM_GRANT, PERM_REVOKE
    uint16_t inheritable = task_perms_table[parent_id].capabilities;
    inheritable &= ~(PERM_PERM_GRANT | PERM_PERM_REVOKE | PERM_KERNEL_MODE);
    
    // Add inherited permissions to child's existing permissions
    task_perms_table[child_id].capabilities |= inheritable;
}

// Get permission name
const char* perm_name(uint16_t perm) {
    for (int i = 0; i < 16; i++) {
        if ((1 << i) == perm) {
            return perm_names[i];
        }
    }
    return "UNKNOWN";
}