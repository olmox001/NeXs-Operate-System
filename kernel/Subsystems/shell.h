/*
 * shell.h - Interactive Kernel Shell
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 *
 * Implements a simple command-line interface for:
 * - Debugging and System Inspection
 * - Process Management
 * - Memory Analysis
 */

#ifndef SHELL_H
#define SHELL_H

#include "kernel.h"

// =============================================================================
// Configuration
// =============================================================================
#define SHELL_CMD_MAX       128     // Max command length
#define SHELL_HISTORY_SIZE  16      // History buffer depth

// =============================================================================
// API
// =============================================================================

/**
 * Initialize Shell Subsystem
 * Sets up buffers and UI state.
 */
void shell_init(void);

/**
 * Main Shell Loop
 * Blocking loop that reads keyboard input and parses commands.
 * Should be run in a dedicated task (e.g. PID 1).
 */
void shell_run(void);

/**
 * Execute a single command line string.
 * Used internally and for scripting/initial startup commands.
 * 
 * @param cmd Null-terminated command string
 */
void shell_execute(const char* cmd);

#endif // SHELL_H
