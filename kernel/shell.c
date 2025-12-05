/*
 * shell.c - Interactive Kernel Shell Implementation
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

#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "libc.h"
#include "buddy.h"
#include "messages.h"
#include "permissions.h"

// Integrity Marker (Used for memory layout verification in kernel.c)
uint64_t __attribute__((section(".data"))) kernel_end_marker = 0xCAFEBABE12345678;

// Command History Buffer
static char cmd_history[SHELL_HISTORY_SIZE][SHELL_CMD_MAX];
static int history_index = 0;
static int history_count = 0;

// Current Input Line
static char cmd_buffer[SHELL_CMD_MAX];
static int cmd_pos = 0;

// Internal Command Prototypes
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_echo(const char* args);
static void cmd_mem(void);
static void cmd_perms(const char* args);
static void cmd_msg(const char* args);
static void cmd_version(void);
static void cmd_uptime(void);

/**
 * Display Command Prompt
 */
static void print_prompt(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("kernel");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("$ ");
}

/**
 * Log command to history
 */
static void add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    
    strncpy(cmd_history[history_index], cmd, SHELL_CMD_MAX - 1);
    cmd_history[history_index][SHELL_CMD_MAX - 1] = '\0';
    
    history_index = (history_index + 1) % SHELL_HISTORY_SIZE;
    if (history_count < SHELL_HISTORY_SIZE) {
        history_count++;
    }
}

/**
 * Initialize Shell State
 */
void shell_init(void) {
    for (int i = 0; i < SHELL_HISTORY_SIZE; i++) {
        cmd_history[i][0] = '\0';
    }
    
    history_index = 0;
    history_count = 0;
    cmd_pos = 0;
    cmd_buffer[0] = '\0';
    
    // Welcome Banner
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("\n=== x86_64 Kernel Shell ===\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Type 'help' for available commands\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/**
 * Main Shell Loop (Blocking)
 */
void shell_run(void) {
    print_prompt();
    
    while (1) {
        char c = keyboard_getchar();
        
        if (c == '\n') {
            // Execute Command
            vga_putc('\n');
            cmd_buffer[cmd_pos] = '\0';
            
            if (cmd_pos > 0) {
                add_to_history(cmd_buffer);
                shell_execute(cmd_buffer);
            }
            
            // Reset Buffer
            cmd_pos = 0;
            cmd_buffer[0] = '\0';
            print_prompt();
            
        } else if (c == '\b') {
            // Handle Backspace
            if (cmd_pos > 0) {
                cmd_pos--;
                vga_putc('\b');
            }
            
        } else if (c >= 32 && c < 127) {
            // Standard Characters
            ASSERT(cmd_pos < SHELL_CMD_MAX);
            if (cmd_pos < SHELL_CMD_MAX - 1) {
                cmd_buffer[cmd_pos++] = c;
                vga_putc(c);
            }
        }
    }
}

/**
 * Command Parser and Dispatcher
 */
void shell_execute(const char* cmd) {
    ASSERT(cmd != NULL);
    
    // Trim Leading Whitespace
    while (*cmd == ' ') cmd++;
    
    if (strlen(cmd) == 0) return;
    
    // Split Command and Arguments
    char cmd_name[32];
    int i = 0;
    while (cmd[i] && cmd[i] != ' ' && i < 31) {
        cmd_name[i] = cmd[i];
        i++;
    }
    cmd_name[i] = '\0';
    
    const char* args = cmd + i;
    while (*args == ' ') args++;
    
    // Dispatch Command
    if (strcmp(cmd_name, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd_name, "clear") == 0) {
        cmd_clear();
    } else if (strcmp(cmd_name, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(cmd_name, "mem") == 0) {
        cmd_mem();
    } else if (strcmp(cmd_name, "perms") == 0) {
        cmd_perms(args);
    } else if (strcmp(cmd_name, "msg") == 0) {
        cmd_msg(args);
    } else if (strcmp(cmd_name, "version") == 0) {
        cmd_version();
    } else if (strcmp(cmd_name, "uptime") == 0) {
        cmd_uptime();
    } else {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("Unknown command: ");
        vga_puts(cmd_name);
        vga_puts("\nType 'help' for available commands\n");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    }
}

/**
 * 'help' - List commands
 */
static void cmd_help(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Available commands:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    vga_puts("  help       - Show this help\n");
    vga_puts("  clear      - Clear screen\n");
    vga_puts("  echo <msg> - Print message\n");
    vga_puts("  mem        - Show memory statistics\n");
    vga_puts("  perms [id] - Show task permissions\n");
    vga_puts("  msg <id>   - Send test message to task\n");
    vga_puts("  version    - Show kernel version\n");
    vga_puts("  uptime     - Show system uptime\n");
}

/**
 * 'clear' - Clear screen
 */
static void cmd_clear(void) {
    vga_clear();
}

/**
 * 'echo' - Print text
 */
static void cmd_echo(const char* args) {
    vga_puts(args);
    vga_putc('\n');
}

/**
 * 'mem' - Memory Status
 */
static void cmd_mem(void) {
    size_t total, used, free_mem;
    buddy_stats(&total, &used, &free_mem);
    
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Memory Statistics:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    vga_puts("  Total:  ");
    vga_puti(total / 1024);
    vga_puts(" KB\n");
    
    vga_puts("  Used:   ");
    vga_puti(used / 1024);
    vga_puts(" KB (");
    vga_puti((used * 100) / total);
    vga_puts("%)\n");
    
    vga_puts("  Free:   ");
    vga_puti(free_mem / 1024);
    vga_puts(" KB (");
    vga_puti((free_mem * 100) / total);
    vga_puts("%)\n");
}

/**
 * 'perms' - Security Audit
 */
static void cmd_perms(const char* args) {
    uint32_t task_id = 0;
    if (strlen(args) > 0) {
        task_id = atoi(args);
    }
    
    if (task_id >= MAX_TASKS) {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("Invalid task ID (must be 0-");
        vga_puti(MAX_TASKS - 1);
        vga_puts(")\n");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }
    
    uint16_t perms = perm_get(task_id);
    
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Task ");
    vga_puti(task_id);
    vga_puts(" Permissions:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    if (perms == PERM_NONE) {
        vga_puts("  (none)\n");
        return;
    }
    
    for (int i = 0; i < 16; i++) {
        uint16_t perm = 1 << i;
        if (perms & perm) {
            vga_puts("  ");
            vga_puts(perm_name(perm));
            vga_putc('\n');
        }
    }
}

/**
 * 'msg' - IPC Test
 */
static void cmd_msg(const char* args) {
    uint32_t target_id = atoi(args);
    
    if (target_id >= MAX_TASKS) {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("Invalid task ID\n");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }
    
    const char* msg_data = "Hello from shell!";
    int result = msg_send(0, target_id, MSG_TYPE_DATA, msg_data, strlen(msg_data));
    
    if (result == 0) {
        vga_set_color(VGA_GREEN, VGA_BLACK);
        vga_puts("Message sent to task ");
        vga_puti(target_id);
        vga_putc('\n');
        vga_set_color(VGA_WHITE, VGA_BLACK);
    } else {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("Failed to send message\n");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    }
}

/**
 * 'version' - Kernel Stats
 */
static void cmd_version(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("x86_64 Kernel Version ");
    vga_puts(KERNEL_VERSION);
    vga_puts("\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("Built: ");
    vga_puts(__DATE__);
    vga_puts(" ");
    vga_puts(__TIME__);
    vga_putc('\n');
}

/**
 * 'uptime' - Placeholder
 */
static void cmd_uptime(void) {
    vga_puts("System uptime: 0 seconds\n");
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    vga_puts("(Timer not yet implemented)\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
};
