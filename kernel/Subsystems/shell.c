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
static void cmd_help(const char* args);
static void cmd_clear(const char* args);
static void cmd_echo(const char* args);
static void cmd_mem(const char* args);
static void cmd_perms(const char* args);
static void cmd_msg(const char* args);
static void cmd_version(const char* args);
static void cmd_uptime(const char* args);
static void cmd_tasks(const char* args);
static void cmd_pid(const char* args);
static void cmd_sleep(const char* args);
static void cmd_priority(const char* args);
static void cmd_reboot(const char* args);
static void cmd_halt(const char* args);
static void cmd_uid(const char* args);

// =============================================================================
// Command Table (Sorted Alphabetically for Binary Search)
// =============================================================================
struct shell_cmd_entry {
    const char* name;
    void (*func)(const char* args);
};

static const struct shell_cmd_entry shell_cmds[] = {
    {"clear",    cmd_clear},
    {"echo",     cmd_echo},
    {"halt",     cmd_halt},
    {"help",     cmd_help},
    {"mem",      cmd_mem},
    {"msg",      cmd_msg},
    {"perms",    cmd_perms},
    {"pid",      cmd_pid},
    {"priority", cmd_priority},
    {"reboot",   cmd_reboot},
    {"sleep",    cmd_sleep},
    {"tasks",    cmd_tasks},
    {"uid",      cmd_uid},
    {"uptime",   cmd_uptime},
    {"version",  cmd_version},
};

static const int shell_cmd_count = sizeof(shell_cmds) / sizeof(shell_cmds[0]);

// =============================================================================
// Helpers
// =============================================================================

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
        // Blocking read (yields if no input)
        char c = keyboard_getchar();

        if (c == '\n') {
            // Execute
            vga_putc('\n');
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
                vga_putc('\b'); // Handle visual backspace (vga driver support)
            }
        } else if (c >= 32 && c < 127) {
            // Printable Characters
            if (cmd_pos < SHELL_CMD_MAX - 1) {
                cmd_buffer[cmd_pos++] = c;
                vga_putc(c);
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

    // Dispatch using Binary Search
    int left = 0;
    int right = shell_cmd_count - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = strcmp(cmd_name, shell_cmds[mid].name);

        if (cmp == 0) {
            shell_cmds[mid].func(args);
            return;
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    // Not found
    vga_set_color(VGA_RED, VGA_BLACK);
    vga_puts("Unknown command: ");
    vga_puts(cmd_name);
    vga_puts("\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

// =============================================================================
// Commands
// =============================================================================

static void cmd_help(const char* args) {
    (void)args;
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Available commands:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  help           - Show this list\n");
    vga_puts("  clear          - Clear screen\n");
    vga_puts("  echo <text>    - Print text\n");
    vga_puts("  mem            - Memory usage stats\n");
    vga_puts("  tasks          - List running processes\n");
    vga_puts("  pid            - Show shell PID\n");
    vga_puts("  uptime         - System uptime\n");
    vga_puts("  sleep <ms>     - Sleep for X milliseconds\n");
    vga_puts("  priority <val> - Set current task priority\n");
    vga_puts("  perms [pid]    - Show permissions\n");
    vga_puts("  msg <pid>      - Send test message\n");
    vga_puts("  version        - Kernel version info\n");
    vga_puts("  reboot         - System Reset\n");
    vga_puts("  halt           - System Halt\n");
}

static void cmd_clear(const char* args) {
    (void)args;
    vga_clear();
}

static void cmd_echo(const char* args) {
    vga_puts(args);
    vga_putc('\n');
}

static void cmd_mem(const char* args) {
    (void)args;
    size_t total, used, free_mem;
    buddy_stats(&total, &used, &free_mem);

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Memory Statistics:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    // Formatting KB
    vga_puts("  Total: "); vga_puti(total / 1024); vga_puts(" KB\n");

    vga_puts("  Used:  "); vga_puti(used / 1024); vga_puts(" KB (");
    vga_puti(total ? (used * 100) / total : 0); vga_puts("%)\n");

    vga_puts("  Free:  "); vga_puti(free_mem / 1024); vga_puts(" KB (");
    vga_puti(total ? (free_mem * 100) / total : 0); vga_puts("%)\n");
}

static void cmd_tasks(const char* args) {
    (void)args;
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Running Tasks:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    if (!current_task) {
        vga_puts("  (scheduler not active)\n");
        return;
    }

    vga_puts("  PID  STATE     PRIO   CPU   NAME\n");

    // Traverse Circular List
    struct task* t = current_task;
    const char* states[] = {"READY", "RUN", "SLEEP", "WAIT", "BLK", "DEAD"};

    do {
        vga_puts("  ");
        vga_puti(t->pid);
        vga_puts("    ");

        // State String
        int s_idx = t->state;
        if (s_idx > 5) s_idx = 5;
        vga_puts(states[s_idx]);

        // Padding
        if (strlen(states[s_idx]) < 5) vga_puts(" ");
        if (t->pid < 10) vga_puts(" ");

        vga_puts("   ");
        vga_puti(t->priority);
        vga_puts("    ");
        vga_puti((int)(t->cpu_time & 0xFFFF));
        vga_puts("\n");

        t = t->next;
    } while (t != current_task);
}

static void cmd_pid(const char* args) {
    (void)args;
    vga_puts("Current PID: ");
    vga_puti(current_task ? current_task->pid : 0);
    vga_puts("\n");
}

static void cmd_uptime(const char* args) {
    (void)args;
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

static void cmd_version(const char* args) {
    (void)args;
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("NeXs-OS Kernel ");
    vga_puts(KERNEL_VERSION);
    vga_puts("\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("Build: "); vga_puts(__DATE__); vga_puts(" "); vga_puts(__TIME__); vga_puts("\n");
}

static void cmd_reboot(const char* args) {
    (void)args;
    vga_puts("Rebooting...\n");

    // 8042 Keyboard Controller Reset
    outb(0x64, 0xFE);

    // Triple Fault Loop
    while(1) {
        asm volatile("cli; lidt (%0); int3" :: "r" (0));
    }
}

static void cmd_halt(const char* args) {
    (void)args;
    vga_puts("System Halted.\n");
    asm volatile("cli");
    while(1) {
        asm volatile("hlt");
    }
}

static void cmd_uid(const char* args) {
    (void)args;
    vga_puts("Current UID: ");
    vga_puti(current_task ? current_task->uid : 0); // Handle null task
    vga_puts("\n");
}
