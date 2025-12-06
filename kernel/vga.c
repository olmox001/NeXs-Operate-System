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
#include "libc.h"
#include "serial.h" // For Dual Output (VGA + Serial)
#include "kernel.h"

// VGA Memory Buffer Address (Standard Text Mode)
static volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;

// Cursor Position State
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_color = 0x0F; // White on Black

/**
 * Compose a VGA entry from character and color
 */
static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/**
 * Compose a color byte from foreground and background
 */
static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

/**
 * Update the hardware cursor position using VGA IO ports
 */
static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

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
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', current_color);
    }
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
 * Scroll the screen up by one line (optimized)
 */
void vga_scroll(void) {
    // Fast copy using word-sized operations
    volatile uint16_t* dst = vga_buffer;
    volatile uint16_t* src = vga_buffer + VGA_WIDTH;
    int count = VGA_WIDTH * (VGA_HEIGHT - 1);
    
    // Use 64-bit copies for speed (4 characters at a time)
    volatile uint64_t* dst64 = (volatile uint64_t*)dst;
    volatile uint64_t* src64 = (volatile uint64_t*)src;
    int count64 = count / 4;
    for (int i = 0; i < count64; i++) {
        dst64[i] = src64[i];
    }
    
    // Clear bottom line
    uint16_t blank = (uint16_t)' ' | ((uint16_t)current_color << 8);
    dst = vga_buffer + VGA_WIDTH * (VGA_HEIGHT - 1);
    for (int x = 0; x < VGA_WIDTH; x++) {
        dst[x] = blank;
    }
    
    cursor_y = VGA_HEIGHT - 1;
}

/**
 * output a single character to the screen
 * Handles special characters like Newline, Tab, Backspace.
 */
void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3; // Tab align 4
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', current_color);
        }
    } else {
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
 * Output a String
 * Mirrors output to Serial Port for debugging.
 */
void vga_puts(const char* str) {
    // 1. Mirror to Serial Port (Headless Debug)
    serial_puts(str);
    
    // 2. VGA Output
    // Disable interrupts to ensure atomic printing (no race conditions)
    uint64_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags));
    
    const char* s = str;
    while (*s) {
        vga_putc(*s++);
    }
    
    // Restore Interrupts if they were enabled
    if (flags & 0x200) asm volatile("sti");
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
 * Move Cursor Absolute
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