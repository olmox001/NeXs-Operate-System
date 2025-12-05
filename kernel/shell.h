// shell.h - Interactive Shell
#ifndef SHELL_H
#define SHELL_H

#include "kernel.h"

#define SHELL_CMD_MAX 128
#define SHELL_HISTORY_SIZE 16

// Initialize shell
void shell_init(void);

// Run shell (main loop)
void shell_run(void);

// Execute command
void shell_execute(const char* cmd);

#endif // SHELL_H
