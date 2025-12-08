/*
 * module.c - Kernel Module System Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "module.h"
#include "libc.h"
#include "vga.h"

// =============================================================================
// Global Registries
// =============================================================================
static struct module_info* module_list = NULL;
static struct device* device_list = NULL;

// =============================================================================
// Module Management
// =============================================================================

int module_register(struct module_info* mod) {
    if (!mod || !mod->name[0]) return -1;
    
    // Prevent duplicates
    if (module_find(mod->name)) return -1;
    
    // Initialize State
    mod->state = MOD_STATE_UNLOADED;
    
    // Append to list (Prepend is O(1))
    mod->next = module_list;
    module_list = mod;
    
    return 0;
}

struct module_info* module_find(const char* name) {
    struct module_info* m = module_list;
    while (m) {
        if (strcmp(m->name, name) == 0) return m;
        m = m->next;
    }
    return NULL;
}

/**
 * Load a single module.
 * Recursively resolves dependencies.
 */
static int module_load(struct module_info* mod) {
    if (!mod) return -1;
    if (mod->state == MOD_STATE_LOADED) return 0; // Already loaded
    
    // Cycle detection? (Simple guard)
    if (mod->state == MOD_STATE_LOADING) {
        vga_puts("ERROR: Circular dependency detected in ");
        vga_puts(mod->name);
        vga_puts("\n");
        return -1;
    }
    
    mod->state = MOD_STATE_LOADING;
    
    // Check & Load Dependencies
    if (mod->depends) {
        for (int i = 0; mod->depends[i]; i++) {
            struct module_info* dep = module_find(mod->depends[i]);
            
            // Missing Dependency?
            if (!dep) {
                vga_puts("ERROR: Missing dependency ");
                vga_puts(mod->depends[i]);
                vga_puts(" for ");
                vga_puts(mod->name);
                vga_puts("\n");
                mod->state = MOD_STATE_ERROR;
                return -1;
            }
            
            // Load Dependency
            if (module_load(dep) != 0) {
                mod->state = MOD_STATE_ERROR;
                return -1;
            }
        }
    }
    
    // Run Init Callback
    if (mod->init) {
        int ret = mod->init();
        if (ret != 0) {
            vga_puts("ERROR: Module init failed: ");
            vga_puts(mod->name);
            vga_puts("\n");
            mod->state = MOD_STATE_ERROR;
            return ret;
        }
    }
    
    // Success
    mod->state = MOD_STATE_LOADED;
    // vga_puts("[MODULE] Loaded "); vga_puts(mod->name); vga_puts("\n");
    return 0;
}

int module_unregister(const char* name) {
    struct module_info* prev = NULL;
    struct module_info* m = module_list;
    
    while (m) {
        if (strcmp(m->name, name) == 0) {
            // Call Exit Callback
            if (m->state == MOD_STATE_LOADED && m->exit) {
                m->exit();
            }
            m->state = MOD_STATE_UNLOADED;
            
            // Unlink
            if (prev) prev->next = m->next;
            else module_list = m->next;
            
            return 0;
        }
        prev = m;
        m = m->next;
    }
    return -1;
}

void modules_init(void) {
    // Basic dependency resolution strategy:
    // Iterate priority levels from 0 to 5.
    // In each level, try to load all unloaded modules.
    
    vga_puts("[MODULE] Initializing Kernel Modules...\n");
    
    for (int prio = MOD_PRIORITY_CORE; prio <= MOD_PRIORITY_USER; prio++) {
        struct module_info* m = module_list;
        int count = 0;
        while (m) {
            if (m->priority == (uint32_t)prio && m->state == MOD_STATE_UNLOADED) {
                if (module_load(m) == 0) {
                    count++;
                }
            }
            m = m->next;
        }
        // if (count) { vga_puts("  Priority "); vga_puti(prio); vga_puts(": Loaded "); vga_puti(count); vga_puts(" modules\n"); }
    }
}

void modules_list(void) {
    vga_puts("Loaded Modules:\n");
    struct module_info* m = module_list;
    if (!m) vga_puts("  (none)\n");
    
    while (m) {
        vga_puts("  - ");
        vga_puts(m->name);
        vga_puts(" (v");
        vga_puti((m->version >> 16) & 0xFF); vga_puts(".");
        vga_puti((m->version >> 8) & 0xFF);
        vga_puts(") [");
        
        switch (m->state) {
            case MOD_STATE_LOADED: vga_puts("LOADED"); break;
            case MOD_STATE_ERROR: vga_puts("ERROR"); break;
            default: vga_puts("OFF"); break;
        }
        vga_puts("]\n");
        m = m->next;
    }
}

// =============================================================================
// Device Management
// =============================================================================

int device_register(struct device* dev) {
    if (!dev || !dev->name[0]) return -1;
    if (device_find(dev->name)) return -1;
    
    dev->next = device_list;
    device_list = dev;
    return 0;
}

int device_unregister(const char* name) {
    struct device* prev = NULL;
    struct device* d = device_list;
    
    while (d) {
        if (strcmp(d->name, name) == 0) {
            if (prev) prev->next = d->next;
            else device_list = d->next;
            return 0;
        }
        prev = d;
        d = d->next;
    }
    return -1; // Not found
}

struct device* device_find(const char* name) {
    struct device* d = device_list;
    while (d) {
        if (strcmp(d->name, name) == 0) return d;
        d = d->next;
    }
    return NULL;
}
