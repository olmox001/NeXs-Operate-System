/*
 * vga.c - VGA Text Mode Driver Implementation
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

#include "vga.h"
#include "libx.h"
#include "serial.h" // For Dual Output (VGA + Serial)
#include "kernel.h"
#include "Subsystems/spinlock.h"

// =============================================================================
// Internal State
// =============================================================================

// Memory Mapped I/O Address for VGA Text Buffer (Color Monitor)
static volatile uint16_t* vga_mem = (uint16_t*)0xB8000;

// Virtual Console Structure
typedef struct {
    uint16_t buffer[VGA_WIDTH * VGA_HEIGHT];
    int cursor_x;
    int cursor_y;
    uint8_t color;
} vc_t;

static vc_t consoles[VGA_NUM_CONSOLES];
static int active_console = 0; // The one currently on screen

// Current target for vga_puts (legacy support -> Console 0 / Kernel Log)
// We treat vga_puts as writing to Console 0.

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Compose a VGA memory entry.
 * Entry = [Color Byte (8 bits)] [Character Byte (8 bits)]
 */
static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/**
 * Compose a VGA color byte.
 * Color = [Background (4 bits)] [Foreground (4 bits)]
 */
static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

/**
 * Update the hardware cursor position.
 * Uses VGA CRT Controller ports 0x3D4 (Index) and 0x3D5 (Data).
 */
/**
 * Update the hardware cursor position.
 * Uses VGA CRT Controller ports 0x3D4 (Index) and 0x3D5 (Data).
 */
static void update_hardware_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    
    // Register 0x0F: Cursor Location Low Byte
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    
    // Register 0x0E: Cursor Location High Byte
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// =============================================================================
// Driver Implementation
// =============================================================================

/**
 * Initialize VGA Driver
 */
/**
 * Initialize VGA Driver
 */
void vga_init(void) {
    // Initialize all consoles
    for (int i = 0; i < VGA_NUM_CONSOLES; i++) {
        consoles[i].cursor_x = 0;
        consoles[i].cursor_y = 0;
        consoles[i].color = 0x0F; // White on Black
        
        // Clear buffer
        uint16_t empty = vga_entry(' ', consoles[i].color);
        for (int j = 0; j < VGA_WIDTH * VGA_HEIGHT; j++) {
            consoles[i].buffer[j] = empty;
        }
    }
    
    active_console = 0;
    
    // Sync active console to hardware
    vga_set_active_console(0);
}

/**
 * Clear the Screen (Legacy: Clears Console 0)
 */
void vga_clear(void) {
    vga_clear_console(0);
}

void vga_clear_console(int index) {
    if (index < 0 || index >= VGA_NUM_CONSOLES) return;
    
    vc_t* vc = &consoles[index];
    uint16_t empty = vga_entry(' ', vc->color);
    
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vc->buffer[i] = empty;
    }
    
    vc->cursor_x = 0;
    vc->cursor_y = 0;
    
    // If this is the active console, update screen immediately
    if (index == active_console) {
        // Memcpy to VRAM
        memcpy((void*)vga_mem, (void*)vc->buffer, VGA_WIDTH * VGA_HEIGHT * 2);
        update_hardware_cursor(0, 0);
    }
}

/**
 * Set Text Color
 */
/**
 * Set Text Color (Legacy: Sets Console 0 color)
 */
void vga_set_color(uint8_t fg, uint8_t bg) {
    consoles[0].color = vga_color(fg, bg);
}

/**
 * Scroll the screen up by one line.
 * Moves lines 1-24 to 0-23, and clears line 24.
 */
/**
 * Scroll a specific console
 */
static void vga_scroll_vc(vc_t* vc) {
    // 1. Move Memory Up
    uint16_t* dst = vc->buffer;
    uint16_t* src = vc->buffer + VGA_WIDTH;
    int count = VGA_WIDTH * (VGA_HEIGHT - 1);
    
    // Optimization: Use 64-bit copy
    uint64_t* dst64 = (uint64_t*)dst;
    uint64_t* src64 = (uint64_t*)src;
    int count64 = count / 4;
    
    for (int i = 0; i < count64; i++) {
        dst64[i] = src64[i];
    }
    
    // 2. Clear Bottom Line
    uint16_t blank = vga_entry(' ', vc->color);
    dst = vc->buffer + (VGA_WIDTH * (VGA_HEIGHT - 1));
    
    for (int x = 0; x < VGA_WIDTH; x++) {
        dst[x] = blank;
    }
    
    // 3. Keep cursor on last line start
    // Standard terminal behavior usually keeps X where it was or resets to 0. 
    // Let's keep X, reset Y is implied by caller usually.
    // Actually vga_putc calls this when Y >= HEIGHT.
    vc->cursor_y = VGA_HEIGHT - 1;
}

void vga_scroll(void) {
    vga_scroll_vc(&consoles[0]);
    if (active_console == 0) {
        memcpy((void*)vga_mem, consoles[0].buffer, VGA_WIDTH * VGA_HEIGHT * 2);
    }
}

/**
 * Put Character
 * Handles special control characters (newline, CR, tab, backspace).
 */
/**
 * Put Character Internal
 */
static void vga_putc_vc(vc_t* vc, char c) {
    if (c == '\n') {
        vc->cursor_x = 0;
        vc->cursor_y++;
    } else if (c == '\r') {
        vc->cursor_x = 0;
    } else if (c == '\t') {
        vc->cursor_x = (vc->cursor_x + 4) & ~3;
    } else if (c == '\b') {
        if (vc->cursor_x > 0) {
            vc->cursor_x--;
            vc->buffer[vc->cursor_y * VGA_WIDTH + vc->cursor_x] = vga_entry(' ', vc->color);
        } else if (vc->cursor_y > 0) {
             // Wrap back to previous line? Optional. 
             // Standard behavior usually stops at 0.
             // Let's just stop at 0 for now.
        }
    } else {
        vc->buffer[vc->cursor_y * VGA_WIDTH + vc->cursor_x] = vga_entry(c, vc->color);
        vc->cursor_x++;
    }
    
    if (vc->cursor_x >= VGA_WIDTH) {
        vc->cursor_x = 0;
        vc->cursor_y++;
    }
    
    if (vc->cursor_y >= VGA_HEIGHT) {
        vga_scroll_vc(vc);
    }
}

void vga_putc(char c) {
    // Legacy: write to kernel log (Console 0)
    vc_t* vc = &consoles[0];
    vga_putc_vc(vc, c);
    
    if (active_console == 0) {
        // Dirty update? optimizing is hard. Full copy for now or single write.
        // vga_mem access is slow? It's MMIO but usually cached/WC. 
        // Simple safe way: sync the buffer.
        // Or better: write directly to vga_mem if active.
        // Let's write to vga_mem directly index-coordinated.
        memcpy((void*)vga_mem, vc->buffer, VGA_WIDTH * VGA_HEIGHT * 2);
        update_hardware_cursor(vc->cursor_x, vc->cursor_y);
    }
}

/**
 * Put String
 * Also mirrors output to Serial Port for debug purposes.
 */
// Spinlock for thread-safe output
static spinlock_t vga_lock;
// One-time init flag
static int locks_initialized = 0;

void vga_puts(const char* str) {
    // 1. Mirror to Serial Port (Headless Debugging / Logs)
    serial_puts(str);

    vga_write_console(0, str);
}

void vga_write_console(int index, const char* str) {
    if (index < 0 || index >= VGA_NUM_CONSOLES) return;

    if (!locks_initialized) {
        spinlock_init(&vga_lock);
        locks_initialized = 1;
    }
    
    spinlock_acquire(&vga_lock);
    
    vc_t* vc = &consoles[index];
    const char* s = str;
    while (*s) {
        vga_putc_vc(vc, *s++);
    }
    
    // If this is the active console, flush to screen
    if (index == active_console) {
        // memcpy is fast enough for text mode (4KB)
        memcpy((void*)vga_mem, vc->buffer, VGA_WIDTH * VGA_HEIGHT * 2);
        update_hardware_cursor(vc->cursor_x, vc->cursor_y);
    }
    
    spinlock_release(&vga_lock); 
}

void vga_set_active_console(int index) {
    if (index < 0 || index >= VGA_NUM_CONSOLES) return;
    
    spinlock_acquire(&vga_lock);
    active_console = index;
    
    // Full Rewrite
    vc_t* vc = &consoles[index];
    memcpy((void*)vga_mem, vc->buffer, VGA_WIDTH * VGA_HEIGHT * 2);
    update_hardware_cursor(vc->cursor_x, vc->cursor_y);
    
    spinlock_release(&vga_lock);
}

int vga_get_active_console(void) {
    return active_console;
}

/**
 * Print Integer (Decimal)
 */
void vga_puti(int value) {
    char buf[32];
    itoa(value, buf, 10);
    vga_puts(buf);
}

/**
 * Print Integer (Hexadecimal)
 */
void vga_putx(uint32_t value) {
    char buf[32];
    uitoa(value, buf, 16);
    vga_puts("0x");
    vga_puts(buf);
}

/**
 * Set Cursor Absolute Position for Console 0
 */
void vga_set_cursor(int x, int y) {
    vc_t* vc = &consoles[0];
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vc->cursor_x = x;
        vc->cursor_y = y;
        if (active_console == 0) update_hardware_cursor(x, y);
    }
}


/**
 * Get Cursor Position
 */
void vga_get_cursor(int* x, int* y) {
    if (x) *x = consoles[0].cursor_x;
    if (y) *y = consoles[0].cursor_y;
}