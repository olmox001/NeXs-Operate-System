/*
 * handlers.h - IRQ Handler Interface
 *
 * BSD 3-Clause License
 * Copyright (c) 2025, NeXs Operate System
 */

#ifndef HANDLERS_H
#define HANDLERS_H

#include "kernel.h"

// IRQ Subsystem
void irq_init(void);
void irq_install_handler(int irq, void (*handler)(void));
void irq_uninstall_handler(int irq);

// Include timer.h for all timing functions
#include "timer.h"

#endif // HANDLERS_H
