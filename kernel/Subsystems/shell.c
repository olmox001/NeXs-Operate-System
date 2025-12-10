/*
 * shell.c - Interactive Kernel Shell Implementation
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "libx.h"
#include "buddy.h"
#include "messages.h"
#include "permissions.h"
#include "process.h"
#include "handlers.h"
#include "syscall.h"
#include "timer.h" // For sleep/uptime

// =============================================================================
// Global State
// =============================================================================

// Integrity Marker (Canary for memory scanning)
uint64_t __attribute__((section(".data"))) kernel_end_marker = 0xCAFEBABE12345678;

static char cmd_history[SHELL_HISTORY_SIZE][SHELL_CMD_MAX];
static int history_index = 0;
static int history_count = 0;

static char cmd_buffer[SHELL_CMD_MAX];
static int cmd_pos = 0;

static int first_command = 1;

// =============================================================================
// Forward Declarations
// =============================================================================
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_echo(const char* args);
static void cmd_mem(void);
static void cmd_perms(const char* args);
static void cmd_msg(const char* args);
static void cmd_version(void);
static void cmd_uptime(void);
static void cmd_tasks(void);
static void cmd_pid(void);
static void cmd_sleep(const char* args);
static void cmd_priority(const char* args);
static void cmd_reboot(void);
static void cmd_halt(void);

// =============================================================================
// Helpers
// =============================================================================


static void shell_print(const char* str) {
    vga_write_console(1, str);
}

static void shell_puti(int val) {
    char buf[32];
    itoa(val, buf, 10);
    shell_print(buf);
}

static void print_prompt(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    shell_print("kernel");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    shell_print("$ ");
}

static void add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    
    strncpy(cmd_history[history_index], cmd, SHELL_CMD_MAX - 1);
    history_index = (history_index + 1) % SHELL_HISTORY_SIZE;
    
    if (history_count < SHELL_HISTORY_SIZE) {
        history_count++;
    }
}

// =============================================================================
// Core Logic
// =============================================================================

void shell_init(void) {
    // Clear State
    memset(cmd_history, 0, sizeof(cmd_history));
    history_index = 0;
    history_count = 0;
    cmd_pos = 0;
    cmd_buffer[0] = '\0';
    first_command = 1;
    
    // UI Setup
    // UI Setup
    vga_clear_console(1);
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    shell_print("=== NeXs-OS x86_64 Shell ===\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    shell_print("Type 'help' for commands (Alt+F1/F2 to switch cons)\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void shell_run(void) {
    // Wait for Kernel Init to settle (prevents "Ready." overwriting prompt)
    sleep(50);
    
    print_prompt();
    
    while (1) {
        // Non-blocking check to allow yielding
        if (!keyboard_available()) {
            // Optional: Yield to save power/CPU if no key
            // However, keyboard_getchar does halt/yield internally if implemented ideally.
            // But let's verify keyboard_getchar implementation.
            // If it halts CPU, it's fine.
        }

        // Blocking read (yields if no input)
        char c = keyboard_getchar();
        
        if (c == '\n') {
            // Execute
            shell_print("\n");
            cmd_buffer[cmd_pos] = '\0';
            
            if (cmd_pos > 0) {
                add_to_history(cmd_buffer);
                shell_execute(cmd_buffer);
            }
            
            // Reset
            cmd_pos = 0;
            cmd_buffer[0] = '\0';
            print_prompt();
            
        } else if (c == '\b') {
            // Delete
            if (cmd_pos > 0) {
                cmd_pos--;
                shell_print("\b"); // Handle visual backspace
            }
        } else if (c >= 32 && c < 127) {
            // Printable Characters
            if (cmd_pos < SHELL_CMD_MAX - 1) {
                cmd_buffer[cmd_pos++] = c;
                char tmp[2] = {c, 0};
                shell_print(tmp);
            }
        }
    }
}

void shell_execute(const char* cmd) {
    // Skip leading whitespace
    while (*cmd == ' ') cmd++;
    if (strlen(cmd) == 0) return;
    
    // Parse Command Name
    char cmd_name[32];
    int i = 0;
    while (cmd[i] && cmd[i] != ' ' && i < 31) {
        cmd_name[i] = cmd[i];
        i++;
    }
    cmd_name[i] = '\0';
    
    // Parse Arguments
    const char* args = cmd + i;
    while (*args == ' ') args++;
    
    // Dispatch Table
    if      (strcmp(cmd_name, "help") == 0)     cmd_help();
    else if (strcmp(cmd_name, "clear") == 0)    cmd_clear();
    else if (strcmp(cmd_name, "echo") == 0)     cmd_echo(args);
    else if (strcmp(cmd_name, "mem") == 0)      cmd_mem();
    else if (strcmp(cmd_name, "perms") == 0)    cmd_perms(args);
    else if (strcmp(cmd_name, "msg") == 0)      cmd_msg(args);
    else if (strcmp(cmd_name, "version") == 0)  cmd_version();
    else if (strcmp(cmd_name, "uptime") == 0)   cmd_uptime();
    else if (strcmp(cmd_name, "tasks") == 0)    cmd_tasks();
    else if (strcmp(cmd_name, "pid") == 0)      cmd_pid();
    else if (strcmp(cmd_name, "sleep") == 0)    cmd_sleep(args);
    else if (strcmp(cmd_name, "priority") == 0) cmd_priority(args);
    else if (strcmp(cmd_name, "reboot") == 0)   cmd_reboot();
    else if (strcmp(cmd_name, "halt") == 0)     cmd_halt();
    else if (strcmp(cmd_name, "uid") == 0) {
        vga_puts("Current UID: ");
        vga_puti(current_task ? current_task->uid : 0); // Handle null task
        vga_puts("\n");
    }
    else {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("Unknown command: ");
        vga_puts(cmd_name);
        vga_puts("\n");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    }
}

// =============================================================================
// Commands
// =============================================================================

static void cmd_help(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    shell_print("Available commands:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    shell_print("  help           - Show this list\n");
    shell_print("  clear          - Clear screen\n");
    shell_print("  echo <text>    - Print text\n");
    shell_print("  mem            - Memory usage stats\n");
    shell_print("  tasks          - List running processes\n");
    shell_print("  pid            - Show shell PID\n");
    shell_print("  uptime         - System uptime\n");
    shell_print("  sleep <ms>     - Sleep for X milliseconds\n");
    shell_print("  priority <val> - Set current task priority\n");
    shell_print("  perms [pid]    - Show permissions\n");
    shell_print("  msg <pid>      - Send test message\n");
    shell_print("  version        - Kernel version info\n");
    shell_print("  reboot         - System Reset\n");
    shell_print("  halt           - System Halt\n");
}

static void cmd_clear(void) {
    vga_clear_console(1);
}

static void cmd_echo(const char* args) {
    shell_print(args);
    shell_print("\n");
}

static void cmd_mem(void) {
    size_t total, used, free_mem;
    buddy_stats(&total, &used, &free_mem);
    
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    shell_print("Memory Statistics:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    // Formatting KB
    shell_print("  Total: "); shell_puti(total / 1024); shell_print(" KB\n");
    
    shell_print("  Used:  "); shell_puti(used / 1024); shell_print(" KB (");
    shell_puti(total ? (used * 100) / total : 0); shell_print("%)\n");
    
    shell_print("  Free:  "); shell_puti(free_mem / 1024); shell_print(" KB (");
    shell_puti(total ? (free_mem * 100) / total : 0); shell_print("%)\n");
}

static void cmd_tasks(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    shell_print("Running Tasks:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    if (!current_task) {
        shell_print("  (scheduler not active)\n");
        return;
    }
    
    vga_puts("  PID  STATE     PRIO   CPU   NAME\n");
    
    // Traverse Circular List
    struct task* t = current_task;
    const char* states[] = {"READY", "RUN", "SLEEP", "WAIT", "BLK", "DEAD"};
    
    do {
        shell_print("  ");
        shell_puti(t->pid);
        shell_print("    ");
        
        // State String
        int s_idx = t->state;
        if (s_idx > 5) s_idx = 5;
        shell_print(states[s_idx]);
        
        // Padding
        if (strlen(states[s_idx]) < 5) shell_print(" ");
        if (t->pid < 10) shell_print(" ");
        
        shell_print("   ");
        shell_puti(t->priority);
        shell_print("    ");
        shell_puti((int)(t->cpu_time & 0xFFFF));
        shell_print("\n");
        
        t = t->next;
    } while (t != current_task);
}

static void cmd_pid(void) {
    vga_puts("Current PID: ");
    vga_puti(current_task ? current_task->pid : 0);
    vga_puts("\n");
}

static void cmd_uptime(void) {
    uint64_t ms = timer_get_ms();
    uint64_t sec = ms / 1000;
    uint64_t ms_part = ms % 1000;
    
    vga_puts("Uptime: ");
    vga_puti((int)sec); vga_puts(".");
    if (ms_part < 100) vga_puts("0");
    if (ms_part < 10)  vga_puts("0");
    vga_puti((int)ms_part);
    vga_puts("s\n");
    
    vga_puts("TSC Freq: ");
    vga_puti((int)(timer_get_freq() / 1000000));
    vga_puts(" MHz\n");
}

static void cmd_sleep(const char* args) {
    int ms = atoi(args);
    if (ms <= 0) {
        vga_puts("Usage: sleep <ms>\n");
        return;
    }
    vga_puts("Sleeping...\n");
    sleep((uint64_t)ms);
    vga_puts("Woke up!\n");
}

static void cmd_priority(const char* args) {
    int p = atoi(args);
    if (p < 0 || p > 255) {
        vga_puts("Usage: priority <0-255>\n");
        return;
    }
    if (current_task) {
        task_set_priority(current_task, (uint8_t)p);
        vga_puts("Priority updated.\n");
    }
}

static void cmd_perms(const char* args) {
    uint32_t task_id = (strlen(args) > 0) ? (uint32_t)atoi(args) : (current_task ? current_task->pid : 0);
    
    // NOTE: perm_get accesses array by index, which is mapped to PID in simple model
    // But ensure bounds check handled in perm_get or here.
    if (task_id >= MAX_TASKS) {
        vga_puts("Invalid Task ID.\n");
        return;
    }
    
    uint16_t perms = perm_get(task_id);
    
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Task "); vga_puti(task_id); vga_puts(" Permissions:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    if (perms == 0) {
        vga_puts("  (none)\n");
        return;
    }
    
    // Decode bits
    for (int i = 0; i < 16; i++) {
        if (perms & (1 << i)) {
            vga_puts("  - ");
            vga_puts(perm_name(1 << i));
            vga_puts("\n");
        }
    }
}

static void cmd_msg(const char* args) {
    uint32_t target = atoi(args);
    
    const char* payload = "Ping from Shell";
    int ret = msg_send(current_task ? current_task->pid : 0, target, MSG_TYPE_DATA, payload, strlen(payload));
    
    if (ret == 0) {
        vga_set_color(VGA_GREEN, VGA_BLACK);
        vga_puts("Sent.\n");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    } else {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("Send Failed (Queue Full/Invalid ID).\n");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    }
}

static void cmd_version(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("NeXs-OS Kernel ");
    vga_puts(KERNEL_VERSION);
    vga_puts("\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("Build: "); vga_puts(__DATE__); vga_puts(" "); vga_puts(__TIME__); vga_puts("\n");
}

static void cmd_reboot(void) {
    vga_puts("Rebooting...\n");
    
    // 8042 Keyboard Controller Reset
    outb(0x64, 0xFE);
    
    // Triple Fault Loop
    while(1) {
        asm volatile("cli; lidt (%0); int3" :: "r" (0));
    }
}

static void cmd_halt(void) {
    vga_puts("System Halted.\n");
    asm volatile("cli");
    while(1) {
        asm volatile("hlt");
    }
}
