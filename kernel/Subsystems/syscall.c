/*
 * syscall.c - System Call Dispatcher (POSIX-like, INT 0x80)
 *
 * BSD 3-Clause License
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

#include "syscall.h"
#include "vga.h"
#include "serial.h"
#include "keyboard.h"
#include "process.h"
#include "idt.h"
#include "messages.h"
#include "permissions.h"
#include "handlers.h"
#include "buddy.h"
#include "timer.h"

// =============================================================================
// Internal Syscall Implementation Functions
// =============================================================================

/**
 * SYS_WRITE (1)
 * Currently only supports writing to stdout (FD 1) via VGA.
 */
static int64_t sys_write(int fd, const char* buf, size_t len) {
    if (fd != 1) return -1;
    if (!buf) return -1;

    // Cap length to prevent CPU monopoly
    if (len > 1024) len = 1024;

    // 1. Serial Output (Interrupts Enabled)
    serial_write(buf, len);

    // 2. VGA Output (Interrupts Disabled)
    uint64_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags));

    vga_write(buf, len);

    if (flags & 0x200) asm volatile("sti");

    return (int64_t)len;
}

/**
 * SYS_READ (0)
 * Reads a single character from the keyboard buffer.
 * Non-blocking for now. returns 1 if char read, 0 if empty.
 */
static int64_t sys_read(int fd, char* buf, size_t len) {
    (void)fd; (void)len;
    if (!buf) return -1;
    if (!keyboard_available()) return 0;
    *buf = keyboard_getchar();
    return 1;
}

/**
 * SYS_GETPID (20)
 * Returns the Process ID of the current task.
 */
static int64_t sys_getpid(void) {
    return current_task ? current_task->pid : 0;
}

/**
 * SYS_UPTIME (96)
 * Returns system uptime in milliseconds.
 */
static int64_t sys_uptime(void) {
    return (int64_t)timer_get_ms();
}

/**
 * SYS_MEMINFO (97)
 * populates variables with memory stats.
 */
static int64_t sys_meminfo(size_t* total, size_t* used, size_t* free_mem) {
    buddy_stats(total, used, free_mem);
    return 0;
}

/**
 * SYS_YIELD (24)
 * Voluntarily relinquishes the CPU time slice.
 */
static int64_t sys_yield(void) {
    yield();
    return 0;
}

/**
 * SYS_SLEEP (35)
 * Blocks the process for the specified milliseconds.
 */
static int64_t sys_sleep(uint64_t ms) {
    sleep(ms);
    return 0;
}

/**
 * SYS_EXIT (60)
 * Terminates the current process.
 */
static void sys_exit(int code) {
    (void)code;
    exit();
}

/**
 * SYS_MSGSND (71)
 * Sends an IPC message. Permission Checked.
 */
static int64_t sys_msgsnd(uint32_t dest, uint32_t type, uint64_t data) {
    if (!current_task) return -1;
    // Capability Check
    if (!(current_task->perm_mask & PERM_MSG_SEND)) return -1;
    return msg_send(current_task->pid, dest, type, &data, sizeof(data));
}

/**
 * SYS_MSGRCV (72)
 * Check for pending messages. Permission Checked.
 */
static int64_t sys_msgrcv(uint32_t task_id) {
    if (!current_task) return -1;
    if (!(current_task->perm_mask & PERM_MSG_RECEIVE)) return -1;
    return msg_available(task_id) ? 1 : 0;
}

/**
 * SYS_TASKINFO (98)
 * Retrieves state and priority of a process.
 */
static int64_t sys_taskinfo(uint32_t pid, uint32_t* state, uint8_t* priority) {
    // Find task by PID
    if (!current_task) return -1;
    struct task* t = current_task;
    do {
        if (t->pid == pid) {
            if (state) *state = t->state;
            if (priority) *priority = t->priority;
            return 0;
        }
        t = t->next;
    } while (t != current_task);
    return -1; // Not found
}

/**
 * SYS_GETTIME_NS (99)
 * High-precision timestamp.
 */
static int64_t sys_gettime_ns(void) {
    return (int64_t)timer_get_ns();
}

/**
 * SYS_GETFREQ (100)
 * CPU Timer Frequency.
 */
static int64_t sys_getfreq(void) {
    return (int64_t)timer_get_freq();
}

/**
 * Main Dispatcher
 * Called from ISR 128. Uses registers for arguments (System V ABIish).
 * RAX = Syscall Number
 * RDI = Arg 1
 * RSI = Arg 2
 * RDX = Arg 3
 * Returns result in RAX.
 */
void syscall_handler(struct interrupt_frame* frame) {
    if (!frame) return;
    
    uint64_t num = frame->rax;
    uint64_t a1 = frame->rdi;
    uint64_t a2 = frame->rsi;
    uint64_t a3 = frame->rdx;
    
    int64_t ret = -1;
    
    switch (num) {
        case SYS_READ:      ret = sys_read((int)a1, (char*)a2, (size_t)a3); break;
        case SYS_WRITE:     ret = sys_write((int)a1, (const char*)a2, (size_t)a3); break;
        case SYS_GETPID:    ret = sys_getpid(); break;
        case SYS_UPTIME:    ret = sys_uptime(); break;
        case SYS_MEMINFO:   ret = sys_meminfo((size_t*)a1, (size_t*)a2, (size_t*)a3); break;
        case SYS_YIELD:     ret = sys_yield(); break;
        case SYS_SLEEP:     ret = sys_sleep(a1); break;
        case SYS_EXIT:      sys_exit((int)a1); break;
        case SYS_MSGSND:    ret = sys_msgsnd((uint32_t)a1, (uint32_t)a2, a3); break;
        case SYS_MSGRCV:    ret = sys_msgrcv((uint32_t)a1); break;
        case SYS_TASKINFO:  ret = sys_taskinfo((uint32_t)a1, (uint32_t*)a2, (uint8_t*)a3); break;
        case SYS_GETTIME_NS:ret = sys_gettime_ns(); break;
        case SYS_GETFREQ:   ret = sys_getfreq(); break;
        default: ret = -1; break;
    }
    
    frame->rax = (uint64_t)ret; // Store return value
}

// Initialization info
void syscall_init(void) {
    vga_puts("DEBUG: Syscall Mechanism Initialized (INT 0x80)\n");
}

// =============================================================================
// User-Space Wrappers
// =============================================================================
// These macros emit the 'int 0x80' instruction inline.

#define SYSCALL0(num) ({ \
    int64_t r; \
    asm volatile("mov %1,%%rax; int $0x80; mov %%rax,%0" \
        : "=r"(r) : "i"(num) : "rax","rcx","r11"); \
    r; })

#define SYSCALL1(num, a1) ({ \
    int64_t r; \
    asm volatile("mov %1,%%rax; mov %2,%%rdi; int $0x80; mov %%rax,%0" \
        : "=r"(r) : "i"(num), "r"((uint64_t)(a1)) : "rax","rdi","rcx","r11"); \
    r; })

#define SYSCALL2(num, a1, a2) ({ \
    int64_t r; \
    asm volatile("mov %1,%%rax; mov %2,%%rdi; mov %3,%%rsi; int $0x80; mov %%rax,%0" \
        : "=r"(r) : "i"(num), "r"((uint64_t)(a1)), "r"((uint64_t)(a2)) \
        : "rax","rdi","rsi","rcx","r11"); \
    r; })

#define SYSCALL3(num, a1, a2, a3) ({ \
    int64_t r; \
    asm volatile("mov %1,%%rax; mov %2,%%rdi; mov %3,%%rsi; mov %4,%%rdx; int $0x80; mov %%rax,%0" \
        : "=r"(r) : "i"(num), "r"((uint64_t)(a1)), "r"((uint64_t)(a2)), "r"((uint64_t)(a3)) \
        : "rax","rdi","rsi","rdx","rcx","r11"); \
    r; })

// Public API Implementations
ssize_t write(int fd, const void* buf, size_t n) { return SYSCALL3(SYS_WRITE, fd, buf, n); }
ssize_t read(int fd, void* buf, size_t n) { return SYSCALL3(SYS_READ, fd, buf, n); }
int getpid(void) { return (int)SYSCALL0(SYS_GETPID); }
int sched_yield(void) { return (int)SYSCALL0(SYS_YIELD); }
void _exit(int c) { SYSCALL1(SYS_EXIT, c); while(1); }
uint64_t sys_uptime_wrapper(void) { return SYSCALL0(SYS_UPTIME); }
void sys_sleep_wrapper(uint64_t ms) { SYSCALL1(SYS_SLEEP, ms); }

// Legacy Wrappers
void syscall_write(const char* s) { write(1, s, 0); }
void syscall_yield(void) { sched_yield(); }
int syscall_getpid(void) { return getpid(); }
