// vga.c - Implementation
#include "vga.h"
#include "libc.h"
#include "serial.h" // Dual Output
#include "kernel.h" // For types if needed, or stdint if kernel.h is not enough

// Check if types are defined. If not, include stdint.
// Since kernel.h defines types, and we include it (indirectly or directly), checking collision.
// If kernel.h is included via vga.h?
// Let's assume we need kernel.h or stdint.h. 
// Given the previous error in serial.c, we should prefer kernel.h if possible.
// But vga.c uses uint8_t which might be in kernel.h?
// referencing kernel.h content from memory: it had typedef unsigned long long uint64_t;
// It likely has uint8_t too.

static volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_color = 0x0F; // White on black

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

// Update hardware cursor position
static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', current_color);
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = vga_color(fg, bg);
}

void vga_scroll(void) {
    // Move all lines up
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear bottom line
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }
    
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', current_color);
        }
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, current_color);
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
    
    update_cursor();
}

void vga_puts(const char* str) {
    // 1. Mirror to Serial
    serial_puts(str);
    
    // 2. VGA Output
    // Disable interrupts to ensure atomic printing
    uint64_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags));
    
    const char* s = str;
    while (*s) {
        vga_putc(*s++);
    }
    
    // Restore interrupts if they were enabled (Bit 9 = IF)
    if (flags & 0x200) asm volatile("sti");
}

void vga_puti(int value) {
    char buf[32];
    itoa(value, buf, 10);
    vga_puts(buf);
}

void vga_putx(uint32_t value) {
    char buf[32];
    uitoa(value, buf, 16);
    vga_puts("0x");
    vga_puts(buf);
}

void vga_set_cursor(int x, int y) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        cursor_x = x;
        cursor_y = y;
        update_cursor();
    }
}

void vga_get_cursor(int* x, int* y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}