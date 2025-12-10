/*
 * module.h - Kernel Module Interface
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Provides a Linux-like abstraction for dynamically loadable modules and drivers.
 * Supports dependency management, priority loading, and device registration.
 */

#ifndef MODULE_H
#define MODULE_H

#include "kernel.h"

// =============================================================================
// Load Priority Levels
// =============================================================================
// Modules are loaded in this order during boot.
#define MOD_PRIORITY_CORE       0   // Vital: Allocators, Timers, IRQ
#define MOD_PRIORITY_INTERRUPT  1   // Hardware: IDT, PIC, APIC
#define MOD_PRIORITY_DRIVER     2   // IO: Keyboard, VGA, Serial, Disk
#define MOD_PRIORITY_FILESYSTEM 3   // Storage: VFS, FAT32, EXT2
#define MOD_PRIORITY_SERVICE    4   // System: Shell, IPC, Network
#define MOD_PRIORITY_USER       5   // User: Applications, Daemons

// =============================================================================
// Module State
// =============================================================================
#define MOD_STATE_UNLOADED  0
#define MOD_STATE_LOADING   1
#define MOD_STATE_LOADED    2
#define MOD_STATE_ERROR     3

// =============================================================================
// Module Descriptor
// =============================================================================
struct module_info {
    char        name[32];           // Unique Module Name
    char        author[32];         // Author/Maintainer
    uint32_t    version;            // Version (Major.Minor.Patch)
    uint32_t    priority;           // Load Priority Level
    uint32_t    state;              // Current State
    
    // Lifecycle Callbacks
    int  (*init)(void);             // Initialization (Return 0 on success)
    void (*exit)(void);             // Cleanup/Shutdown
    
    // Power Management Hooks (Optional)
    void (*suspend)(void);
    void (*resume)(void);
    
    // Dependencies
    // NULL-terminated array of module names that must be loaded first
    const char** depends;
    
    // Internal Linked List
    struct module_info* next;
};

// =============================================================================
// Helper Macros (Linux-Compatible)
// =============================================================================
#define MODULE_VERSION(major, minor, patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

// Metadata markers (used by build tools, currently no-op in C)
#define MODULE_NAME(x)      static const char __mod_name[] = x
#define MODULE_AUTHOR(x)    static const char __mod_author[] = x
#define MODULE_LICENSE(x)   static const char __mod_license[] = x

// =============================================================================
// Module API
// =============================================================================

/**
 * Register a static module.
 * Called by module initializer functions usually placed in a special section.
 */
int module_register(struct module_info* mod);

/**
 * Unregister and unload a module.
 */
int module_unregister(const char* name);

/**
 * Initialize and load all registered modules.
 * Respects priority order and dependencies.
 */
void modules_init(void);

/**
 * Find a module by name.
 */
struct module_info* module_find(const char* name);

/**
 * Print list of loaded modules to VGA.
 */
void modules_list(void);

// =============================================================================
// Device Driver Abstraction
// =============================================================================

// Device Types
#define DEV_TYPE_CHAR       1   // Stream (Keyboard, Serial)
#define DEV_TYPE_BLOCK      2   // Random Access (Disk, RAMDrive)
#define DEV_TYPE_NET        3   // Network Interface

// Device Operations (VTable)
struct device_ops {
    int  (*open)(void* dev);
    int  (*close)(void* dev);
    ssize_t (*read)(void* dev, void* buf, size_t size);
    ssize_t (*write)(void* dev, const void* buf, size_t size);
    int  (*ioctl)(void* dev, uint32_t cmd, void* arg);
};

// Device Structure
struct device {
    char            name[16];       // Device Node Name (e.g., "startty")
    uint32_t        type;           // DEV_TYPE_XXX
    uint32_t        flags;          // Feature Flags
    struct device_ops* ops;         // Function Pointers
    void*           private_data;   // Driver-specific data
    struct device*  next;           // internal list
};

// Device API
int device_register(struct device* dev);
int device_unregister(const char* name);
struct device* device_find(const char* name);

#endif // MODULE_H
