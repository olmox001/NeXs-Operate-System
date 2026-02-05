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

// =============================================================================
// Internal State
// =============================================================================

// Memory Mapped I/O Address for VGA Text Buffer (Color Monitor)
static volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;

// Cursor State
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_color = 0x0F; // Default: White Foreground (F) on Black Background (0)

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
static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;

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
void vga_init(void) {
    vga_clear();
}

/**
 * Clear the Screen
 */
void vga_clear(void) {
    uint16_t empty = vga_entry(' ', current_color);

    // Fill entire buffer with space character
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = empty;
    }

    // Reset Cursor
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

/**
 * Set Text Color
 */
void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = vga_color(fg, bg);
}

/**
 * Scroll the screen up by one line.
 * Moves lines 1-24 to 0-23, and clears line 24.
 */
void vga_scroll(void) {
    // 1. Move Memory Up
    volatile uint16_t* dst = vga_buffer;
    volatile uint16_t* src = vga_buffer + VGA_WIDTH;
    int count = VGA_WIDTH * (VGA_HEIGHT - 1);

    // Optimization: Use 64-bit copy (4 characters at once)
    volatile uint64_t* dst64 = (volatile uint64_t*)dst;
    volatile uint64_t* src64 = (volatile uint64_t*)src;
    int count64 = count / 4;

    for (int i = 0; i < count64; i++) {
        dst64[i] = src64[i];
    }

    // 2. Clear Bottom Line
    uint16_t blank = vga_entry(' ', current_color);
    dst = vga_buffer + (VGA_WIDTH * (VGA_HEIGHT - 1));

    for (int x = 0; x < VGA_WIDTH; x++) {
        dst[x] = blank;
    }

    // 3. Keep cursor on last line
    cursor_y = VGA_HEIGHT - 1;
}

/**
 * Put Character
 * Handles special control characters (newline, CR, tab, backspace).
 */
void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3; // Align to next multiple of 4
    } else if (c == '\b') {
        // Backspace handling
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', current_color);
        }
    } else {
        // Normal printable character
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, current_color);
        cursor_x++;
    }

    // Handle Line Wrapping
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    // Handle Scrolling
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }

    update_cursor();
}

/**
 * Write a buffer of known length
 * Mirrors to Serial Port and handles atomicity.
 */
void vga_write(const char* str, size_t len) {
    // 1. Mirror to Serial Port (Headless Debugging / Logs)
    serial_write(str, len);

    // 2. Critical Section (Atomic visual update)
    // Disable interrupts to prevent context switches during printing,
    // which could scramble output from multiple tasks.
    uint64_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags));

    for (size_t i = 0; i < len; i++) {
        vga_putc(str[i]);
    }

    // Restore Interrupts from saved flags
    if (flags & 0x200) asm volatile("sti");
}

/**
 * Put String
 * Also mirrors output to Serial Port for debug purposes.
 */
void vga_puts(const char* str) {
    vga_write(str, strlen(str));
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
 * Set Cursor Absolute Position
 */
void vga_set_cursor(int x, int y) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        cursor_x = x;
        cursor_y = y;
        update_cursor();
    }
}

/**
 * Get Cursor Position
 */
void vga_get_cursor(int* x, int* y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}
