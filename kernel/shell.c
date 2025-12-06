/*
 * shell.c - Interactive Kernel Shell
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "libc.h"
#include "buddy.h"
#include "messages.h"
#include "permissions.h"
#include "process.h"
#include "handlers.h"
#include "syscall.h"
#include "timer.h"

// Integrity Marker
uint64_t __attribute__((section(".data"))) kernel_end_marker = 0xCAFEBABE12345678;

// Buffers
static char cmd_history[SHELL_HISTORY_SIZE][SHELL_CMD_MAX];
static int history_index = 0;
static int history_count = 0;
static char cmd_buffer[SHELL_CMD_MAX];
static int cmd_pos = 0;

// Prototypes
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

static void print_prompt(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("kernel");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("$ ");
}

static void add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    strncpy(cmd_history[history_index], cmd, SHELL_CMD_MAX - 1);
    history_index = (history_index + 1) % SHELL_HISTORY_SIZE;
    if (history_count < SHELL_HISTORY_SIZE) history_count++;
}

static int first_command = 1;

void shell_init(void) {
    for (int i = 0; i < SHELL_HISTORY_SIZE; i++) cmd_history[i][0] = '\0';
    history_index = 0;
    history_count = 0;
    cmd_pos = 0;
    cmd_buffer[0] = '\0';
    first_command = 1;  // Auto-clear on first command
    
    // Clear screen for fresh start
    vga_clear();
    
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("=== NeXs-OS x86_64 Shell ===\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Type 'help' for commands\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void shell_run(void) {
    print_prompt();
    
    while (1) {
        char c = keyboard_getchar();
        
        if (c == '\n') {
            vga_putc('\n');
            cmd_buffer[cmd_pos] = '\0';
            if (cmd_pos > 0) {
                add_to_history(cmd_buffer);
                shell_execute(cmd_buffer);
            }
            cmd_pos = 0;
            cmd_buffer[0] = '\0';
            print_prompt();
        } else if (c == '\b') {
            if (cmd_pos > 0) {
                cmd_pos--;
                vga_putc('\b');
            }
        } else if (c >= 32 && c < 127 && cmd_pos < SHELL_CMD_MAX - 1) {
            cmd_buffer[cmd_pos++] = c;
            vga_putc(c);
        }
    }
}

void shell_execute(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (strlen(cmd) == 0) return;
    
    char cmd_name[32];
    int i = 0;
    while (cmd[i] && cmd[i] != ' ' && i < 31) {
        cmd_name[i] = cmd[i];
        i++;
    }
    cmd_name[i] = '\0';
    
    const char* args = cmd + i;
    while (*args == ' ') args++;
    
    // Command Dispatch
    if (strcmp(cmd_name, "help") == 0) cmd_help();
    else if (strcmp(cmd_name, "clear") == 0) cmd_clear();
    else if (strcmp(cmd_name, "echo") == 0) cmd_echo(args);
    else if (strcmp(cmd_name, "mem") == 0) cmd_mem();
    else if (strcmp(cmd_name, "perms") == 0) cmd_perms(args);
    else if (strcmp(cmd_name, "msg") == 0) cmd_msg(args);
    else if (strcmp(cmd_name, "version") == 0) cmd_version();
    else if (strcmp(cmd_name, "uptime") == 0) cmd_uptime();
    else if (strcmp(cmd_name, "tasks") == 0) cmd_tasks();
    else if (strcmp(cmd_name, "pid") == 0) cmd_pid();
    else if (strcmp(cmd_name, "sleep") == 0) cmd_sleep(args);
    else if (strcmp(cmd_name, "priority") == 0) cmd_priority(args);
    else if (strcmp(cmd_name, "reboot") == 0) cmd_reboot();
    else if (strcmp(cmd_name, "halt") == 0) cmd_halt();
    else if (strcmp(cmd_name, "uid") == 0) {
        vga_puts("Current UID: ");
        vga_puti(current_task ? current_task->uid : 0);
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

// --- Commands ---

static void cmd_help(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Available commands:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  help         - This help\n");
    vga_puts("  clear        - Clear screen\n");
    vga_puts("  echo <msg>   - Print message\n");
    vga_puts("  mem          - Memory statistics\n");
    vga_puts("  tasks        - List running tasks\n");
    vga_puts("  pid          - Show current PID\n");
    vga_puts("  uptime       - System uptime\n");
    vga_puts("  sleep <ms>   - Sleep for milliseconds\n");
    vga_puts("  priority <p> - Set shell priority (0-255)\n");
    vga_puts("  perms [id]   - Show task permissions\n");
    vga_puts("  msg <id>     - Send test message\n");
    vga_puts("  version      - Kernel version\n");
    vga_puts("  reboot       - Reboot system\n");
    vga_puts("  halt         - Halt system\n");
}

static void cmd_clear(void) { vga_clear(); }

static void cmd_echo(const char* args) {
    vga_puts(args);
    vga_putc('\n');
}

static void cmd_mem(void) {
    size_t total, used, free_mem;
    buddy_stats(&total, &used, &free_mem);
    
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Memory Statistics:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    vga_puts("  Total: "); vga_puti(total / 1024); vga_puts(" KB\n");
    vga_puts("  Used:  "); vga_puti(used / 1024); vga_puts(" KB (");
    vga_puti(total ? (used * 100) / total : 0); vga_puts("%)\n");
    vga_puts("  Free:  "); vga_puti(free_mem / 1024); vga_puts(" KB (");
    vga_puti(total ? (free_mem * 100) / total : 0); vga_puts("%)\n");
}

static void cmd_tasks(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Running Tasks:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    if (!current_task) {
        vga_puts("  (no tasks)\n");
        return;
    }
    
    vga_puts("  PID  STATE     PRIO  CPU\n");
    struct task* t = current_task;
    const char* states[] = {"READY", "RUNNING", "SLEEPING", "WAITING", "DEAD"};
    do {
        vga_puts("  ");
        vga_puti(t->pid);
        vga_puts("    ");
        vga_puts(t->state < 5 ? states[t->state] : "???");
        vga_puts("   ");
        vga_puti(t->priority);
        vga_puts("    ");
        vga_puti((int)(t->cpu_time & 0xFFFF));
        vga_puts("\n");
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
    vga_puti((int)sec);
    vga_puts(".");
    if (ms_part < 100) vga_puts("0");
    if (ms_part < 10) vga_puts("0");
    vga_puti((int)ms_part);
    vga_puts("s (TSC: ");
    vga_puti((int)(timer_get_freq() / 1000000));
    vga_puts(" MHz)\n");
}

static void cmd_sleep(const char* args) {
    int ms = atoi(args);
    if (ms <= 0) {
        vga_puts("Usage: sleep <milliseconds>\n");
        return;
    }
    vga_puts("Sleeping for ");
    vga_puti(ms);
    vga_puts(" ms...\n");
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
        vga_puts("Priority set to ");
        vga_puti(p);
        vga_puts("\n");
    }
}

static void cmd_perms(const char* args) {
    uint32_t task_id = strlen(args) > 0 ? atoi(args) : 0;
    if (task_id >= MAX_TASKS) {
        vga_puts("Invalid task ID\n");
        return;
    }
    
    uint16_t perms = perm_get(task_id);
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Task "); vga_puti(task_id); vga_puts(" Permissions:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    if (perms == PERM_NONE) {
        vga_puts("  (none)\n");
        return;
    }
    for (int i = 0; i < 16; i++) {
        if (perms & (1 << i)) {
            vga_puts("  ");
            vga_puts(perm_name(1 << i));
            vga_putc('\n');
        }
    }
}

static void cmd_msg(const char* args) {
    uint32_t target = atoi(args);
    if (target >= MAX_TASKS) {
        vga_puts("Invalid task ID\n");
        return;
    }
    
    const char* data = "Hello from shell!";
    if (msg_send(current_task ? current_task->pid : 0, target, MSG_TYPE_DATA, data, strlen(data)) == 0) {
        vga_set_color(VGA_GREEN, VGA_BLACK);
        vga_puts("Message sent to task "); vga_puti(target); vga_puts("\n");
    } else {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("Failed to send message\n");
    }
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

static void cmd_version(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("NeXs-OS Kernel ");
    vga_puts(KERNEL_VERSION);
    vga_puts("\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("Built: "); vga_puts(__DATE__); vga_puts(" "); vga_puts(__TIME__); vga_puts("\n");
}

static void cmd_reboot(void) {
    vga_puts("Rebooting...\n");
    // Triple fault to reboot
    outb(0x64, 0xFE);
    while(1) hlt();
}

static void cmd_halt(void) {
    vga_puts("System halted.\n");
    cli();
    while(1) hlt();
}
