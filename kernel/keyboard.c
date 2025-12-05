// keyboard.c - Implementation
#include "keyboard.h"
#include "idt.h"

// PS/2 keyboard scancode set 1 to ASCII
static const char scancode_to_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char scancode_to_ascii_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Keyboard state
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_read_pos = 0;
static volatile uint32_t kbd_write_pos = 0;
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock = false;

// Keyboard IRQ handler (called from IRQ1)
void keyboard_handler(void) {
    uint8_t scancode = inb(0x60);
    
    // Handle special keys
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return;
    }
    if (scancode == 0x1D) {
        ctrl_pressed = true;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = false;
        return;
    }
    if (scancode == 0x38) {
        alt_pressed = true;
        return;
    }
    if (scancode == 0xB8) {
        alt_pressed = false;
        return;
    }
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }
    
    // Ignore key release events (bit 7 set)
    if (scancode & 0x80) {
        return;
    }
    
    // Convert scancode to ASCII
    char c = 0;
    if (shift_pressed || caps_lock) {
        c = scancode_to_ascii_shift[scancode];
        if (caps_lock && !shift_pressed && c >= 'A' && c <= 'Z') {
            c = scancode_to_ascii[scancode];
        }
    } else {
        c = scancode_to_ascii[scancode];
    }
    
    if (c) {
        // Add to buffer
        uint32_t next_pos = (kbd_write_pos + 1) % KBD_BUFFER_SIZE;
        if (next_pos != kbd_read_pos) {
            kbd_buffer[kbd_write_pos] = c;
            kbd_write_pos = next_pos;
        }
    }
}

void keyboard_init(void) {
    kbd_read_pos = 0;
    kbd_write_pos = 0;
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    caps_lock = false;
    
    // Unmask IRQ1 (keyboard)
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 1);
    outb(0x21, mask);
}

char keyboard_getchar(void) {
    // Wait for character
    while (kbd_read_pos == kbd_write_pos) {
        hlt(); // Sleep until interrupt
    }
    
    char c = kbd_buffer[kbd_read_pos];
    kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;
    
    ASSERT(kbd_read_pos < KBD_BUFFER_SIZE);
    
    return c;
}

bool keyboard_available(void) {
    return kbd_read_pos != kbd_write_pos;
}

void keyboard_clear(void) {
    kbd_read_pos = 0;
    kbd_write_pos = 0;
}