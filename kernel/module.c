/*
 * module.c - Kernel Module System Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "module.h"
#include "libc.h"
#include "vga.h"

// Module registry
static struct module_info* module_list = NULL;
static struct device* device_list = NULL;

// Register a module
int module_register(struct module_info* mod) {
    if (!mod || !mod->name[0]) return -1;
    
    // Check for duplicate
    if (module_find(mod->name)) return -1;
    
    mod->state = MOD_STATE_UNLOADED;
    mod->next = module_list;
    module_list = mod;
    
    return 0;
}

// Find module by name
struct module_info* module_find(const char* name) {
    struct module_info* m = module_list;
    while (m) {
        if (strcmp(m->name, name) == 0) return m;
        m = m->next;
    }
    return NULL;
}

// Load a single module
static int module_load(struct module_info* mod) {
    if (!mod || mod->state == MOD_STATE_LOADED) return 0;
    
    mod->state = MOD_STATE_LOADING;
    
    // Check dependencies
    if (mod->depends) {
        for (int i = 0; mod->depends[i]; i++) {
            struct module_info* dep = module_find(mod->depends[i]);
            if (!dep || dep->state != MOD_STATE_LOADED) {
                mod->state = MOD_STATE_ERROR;
                return -1;
            }
        }
    }
    
    // Call init
    if (mod->init) {
        int ret = mod->init();
        if (ret != 0) {
            mod->state = MOD_STATE_ERROR;
            return ret;
        }
    }
    
    mod->state = MOD_STATE_LOADED;
    return 0;
}

// Unload a module
int module_unregister(const char* name) {
    struct module_info* prev = NULL;
    struct module_info* m = module_list;
    
    while (m) {
        if (strcmp(m->name, name) == 0) {
            if (m->exit) m->exit();
            
            if (prev) prev->next = m->next;
            else module_list = m->next;
            
            return 0;
        }
        prev = m;
        m = m->next;
    }
    return -1;
}

// Load all modules by priority
void modules_init(void) {
    // Load in priority order (0 first)
    for (int prio = MOD_PRIORITY_CORE; prio <= MOD_PRIORITY_USER; prio++) {
        struct module_info* m = module_list;
        while (m) {
            if (m->priority == (uint32_t)prio && m->state == MOD_STATE_UNLOADED) {
                module_load(m);
            }
            m = m->next;
        }
    }
}

// List all modules
void modules_list(void) {
    vga_puts("Loaded modules:\n");
    struct module_info* m = module_list;
    while (m) {
        vga_puts("  ");
        vga_puts(m->name);
        vga_puts(" (");
        vga_puti(m->state);
        vga_puts(")\n");
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
    return -1;
}

struct device* device_find(const char* name) {
    struct device* d = device_list;
    while (d) {
        if (strcmp(d->name, name) == 0) return d;
        d = d->next;
    }
    return NULL;
}
