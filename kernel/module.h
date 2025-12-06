/*
 * module.h - Kernel Module Interface (Linux-like)
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Provides abstraction for dynamically loadable modules/drivers.
 * Designed for future compatibility with Linux driver patterns.
 */

#ifndef MODULE_H
#define MODULE_H

#include "kernel.h"

// =============================================================================
// Module Priority (Load Order)
// =============================================================================
#define MOD_PRIORITY_CORE       0   // Memory, Timer (first)
#define MOD_PRIORITY_INTERRUPT  1   // IDT, PIC
#define MOD_PRIORITY_DRIVER     2   // Keyboard, VGA, Serial
#define MOD_PRIORITY_FILESYSTEM 3   // VFS, FAT
#define MOD_PRIORITY_SERVICE    4   // Shell, IPC
#define MOD_PRIORITY_USER       5   // User applications

// Module State
#define MOD_STATE_UNLOADED  0
#define MOD_STATE_LOADING   1
#define MOD_STATE_LOADED    2
#define MOD_STATE_ERROR     3

// =============================================================================
// Module Descriptor
// =============================================================================
struct module_info {
    char        name[32];           // Module name
    char        author[32];         // Author/maintainer
    uint32_t    version;            // Version (major.minor.patch packed)
    uint32_t    priority;           // Load order
    uint32_t    state;              // Current state
    
    // Lifecycle callbacks
    int  (*init)(void);             // Called on load (return 0 = success)
    void (*exit)(void);             // Called on unload
    
    // Optional hooks
    void (*suspend)(void);          // Power management
    void (*resume)(void);
    
    // Dependencies (NULL-terminated list)
    const char** depends;
    
    // Linked list
    struct module_info* next;
};

// =============================================================================
// Linux-like Macros
// =============================================================================
#define MODULE_VERSION(major, minor, patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

#define MODULE_NAME(x)      static const char __mod_name[] = x
#define MODULE_AUTHOR(x)    static const char __mod_author[] = x
#define MODULE_LICENSE(x)   static const char __mod_license[] = x

// =============================================================================
// Module Registration
// =============================================================================

// Register a module (called at boot or runtime)
int module_register(struct module_info* mod);

// Unregister and unload a module
int module_unregister(const char* name);

// Load all modules by priority order
void modules_init(void);

// Find module by name
struct module_info* module_find(const char* name);

// List all modules
void modules_list(void);

// =============================================================================
// Driver Abstraction (built on modules)
// =============================================================================

// Generic Device Types
#define DEV_TYPE_CHAR       1   // Character device (keyboard, serial)
#define DEV_TYPE_BLOCK      2   // Block device (disk)
#define DEV_TYPE_NET        3   // Network device

struct device_ops {
    int  (*open)(void* dev);
    int  (*close)(void* dev);
    ssize_t (*read)(void* dev, void* buf, size_t size);
    ssize_t (*write)(void* dev, const void* buf, size_t size);
    int  (*ioctl)(void* dev, uint32_t cmd, void* arg);
};

struct device {
    char            name[16];
    uint32_t        type;
    uint32_t        flags;
    struct device_ops* ops;
    void*           private_data;
    struct device*  next;
};

// Device Registration
int device_register(struct device* dev);
int device_unregister(const char* name);
struct device* device_find(const char* name);

#endif // MODULE_H
