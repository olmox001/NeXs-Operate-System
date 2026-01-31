/*
 * serial.c - Serial Port Implementation (UART 16550A)
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

#include "serial.h"
#include "kernel.h"

// Standard COM1 Base IO Address
#define PORT 0x3f8

// =============================================================================
// Internal Configuration
// =============================================================================

/**
 * Initialize Hardware
 * Configures UART for 38400 baud, 8N1 (8 bits, No parity, 1 stop bit).
 */
static int init_serial(void) {
    outb(PORT + 1, 0x00);    // Disable all UART interrupts (Polling mode for output)
    outb(PORT + 3, 0x80);    // Enable DLAB (Divisor Latch Access Bit)
                             // This allows accessing the baud rate divisor registers

    // Set Baud Rate to 38400 (Max is 115200)
    // Divisor = 115200 / 38400 = 3
    outb(PORT + 0, 0x03);    // Set divisor low byte
    outb(PORT + 1, 0x00);    // Set divisor high byte

    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit (Line Protocol)
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
                             // (0xC7 = 11000111)

    outb(PORT + 4, 0x0B);    // Enable IRQs, RTS/DSR set
                             // (0x0B = 00001011)
    return 0;
}

/**
 * Check Transmit Status
 * Reads Line Status Register (Port + 5). Bit 5 is 'Empty Transmitter Holding Register'.
 */
static int is_transmit_empty(void) {
    return inb(PORT + 5) & 0x20;
}

// =============================================================================
// Public API
// =============================================================================

/**
 * Write a single character
 * Blocking loop until hardware buffer has space.
 */
void serial_putc(char a) {
    while (is_transmit_empty() == 0) {
        // Spin
        asm volatile("pause");
    }
    outb(PORT, a);
}

/**
 * Write a string
 */
void serial_puts(const char* str) {
    while (*str) {
        serial_putc(*str++);
    }
}

/**
 * Write a buffer of known length
 */
void serial_write(const char* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        serial_putc(buf[i]);
    }
}

/**
 * Initialize Driver Interface
 */
void serial_init(void) {
    init_serial();
    serial_puts("\n[SERIAL] Serial Port Initialized\n");
}
