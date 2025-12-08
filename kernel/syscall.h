/*
 * syscall.h - System Call Interface (POSIX-like, INT 0x80)
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include "kernel.h"
#include "idt.h"

// =============================================================================
// System Call Numbers
// =============================================================================
// These correspond to standard Linux x86_64 syscall numbers where possible,
// or custom NeXs OS extensions.
#define SYS_READ        0       // Read from file descriptor
#define SYS_WRITE       1       // Write to file descriptor
#define SYS_GETPID      20      // Get Process ID
#define SYS_YIELD       24      // Yield processor time
#define SYS_SLEEP       35      // Sleep for milliseconds
#define SYS_EXIT        60      // Terminate process
#define SYS_MSGSND      71      // IPC: Send Message
#define SYS_MSGRCV      72      // IPC: Receive Message
#define SYS_UPTIME      96      // Get System Uptime (ms)
#define SYS_MEMINFO     97      // Get Memory Statistics
#define SYS_TASKINFO    98      // Get Task Status
#define SYS_GETTIME_NS  99      // Get High-Resolution Time (ns)
#define SYS_GETFREQ     100     // Get TSC Frequency (Hz)

// =============================================================================
// Initialization
// =============================================================================

// Initialize Syscall Logging/State
void syscall_init(void);

// =============================================================================
// Handlers
// =============================================================================

// Main Dispatcher called by ISR 128 (ints.asm)
void syscall_handler(struct interrupt_frame* frame);

// =============================================================================
// User-Space Wrappers (LibC)
// =============================================================================
// These look like standard POSIX functions but wrap the INT 0x80 instruction.

// File I/O
ssize_t write(int fd, const void* buf, size_t n);
ssize_t read(int fd, void* buf, size_t n);

// Process Control
int getpid(void);
int sched_yield(void);
void _exit(int code);

// Time
uint64_t sys_uptime_wrapper(void);
void sys_sleep_wrapper(uint64_t ms);

// Legacy/Convenience
void syscall_write(const char* s);
void syscall_yield(void);
int syscall_getpid(void);

#endif // SYSCALL_H
