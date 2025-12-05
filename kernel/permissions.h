
// permissions.h - Royalty-Based Dynamic Permission System
#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include "kernel.h"

// Permission flags (capabilities)
#define PERM_NONE           0x0000
#define PERM_MEMORY_ALLOC   0x0001  // Can allocate memory
#define PERM_MEMORY_FREE    0x0002  // Can free memory
#define PERM_IO_READ        0x0004  // Can read I/O ports
#define PERM_IO_WRITE       0x0008  // Can write I/O ports
#define PERM_MSG_SEND       0x0010  // Can send messages
#define PERM_MSG_RECEIVE    0x0020  // Can receive messages
#define PERM_IRQ_INSTALL    0x0040  // Can install IRQ handlers
#define PERM_IRQ_REMOVE     0x0080  // Can remove IRQ handlers
#define PERM_TASK_CREATE    0x0100  // Can create tasks
#define PERM_TASK_DESTROY   0x0200  // Can destroy tasks
#define PERM_PERM_GRANT     0x0400  // Can grant permissions to others
#define PERM_PERM_REVOKE    0x0800  // Can revoke permissions
#define PERM_KERNEL_MODE    0x1000  // Full kernel access
#define PERM_SHELL_ACCESS   0x2000  // Can use shell
#define PERM_DEBUG          0x4000  // Can use debug features
#define PERM_ADMIN          0x8000  // Administrative privileges

// Task permission entry
struct task_perms {
    uint32_t task_id;           // Task ID
    uint16_t capabilities;      // Permission flags
    uint32_t parent_id;         // Parent task (for inheritance)
    uint64_t granted_time;      // When permissions were granted
    bool active;                // Is task active
};

// Initialize permission system
void perm_init(void);

// Create task with initial permissions
int perm_create_task(uint32_t task_id, uint32_t parent_id, uint16_t initial_perms);

// Destroy task and revoke all permissions
void perm_destroy_task(uint32_t task_id);

// Grant permission to task (requires PERM_PERM_GRANT)
int perm_grant(uint32_t granter_id, uint32_t target_id, uint16_t perms);

// Revoke permission from task (requires PERM_PERM_REVOKE)
int perm_revoke(uint32_t revoker_id, uint32_t target_id, uint16_t perms);

// Check if task has specific permission
bool perm_check(uint32_t task_id, uint16_t perm);

// Get all permissions for task
uint16_t perm_get(uint32_t task_id);

// Inherit permissions from parent (called automatically on task creation)
void perm_inherit(uint32_t child_id, uint32_t parent_id);

// Get permission name (for debugging)
const char* perm_name(uint16_t perm);

#endif // PERMISSIONS_H