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

// System Call Numbers
#define SYS_READ        0
#define SYS_WRITE       1
#define SYS_GETPID      20
#define SYS_YIELD       24
#define SYS_SLEEP       35
#define SYS_EXIT        60
#define SYS_MSGSND      71
#define SYS_MSGRCV      72
#define SYS_UPTIME      96
#define SYS_MEMINFO     97
#define SYS_TASKINFO    98

// Init
void syscall_init(void);

// Handler
void syscall_handler(struct interrupt_frame* frame);

// POSIX Wrappers
ssize_t write(int fd, const void* buf, size_t n);
ssize_t read(int fd, void* buf, size_t n);
int getpid(void);
int sched_yield(void);
void _exit(int code);
uint64_t sys_uptime_wrapper(void);
void sys_sleep_wrapper(uint64_t ms);

// Legacy
void syscall_write(const char* s);
void syscall_yield(void);
int syscall_getpid(void);

#endif // SYSCALL_H
