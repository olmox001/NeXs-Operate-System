# NeXs OS Function Map

This document provides a comprehensive mapping of all functions and major labels within the NeXs OS codebase, organized by file.

## Bootloader

### `boot/stage1.asm`
- **_start**: Entry point for the MBR bootloader.
- **print**: Prints a null-terminated string to the screen using BIOS Teletype.
- **error_int13**: Error handler for lack of LBA support.
- **error_disk**: Error handler for disk read failure.
- **error_s2**: Error handler for Stage2 signature mismatch.
- **halt**: Infinite loop halming the CPU.

### `boot/stage2.asm`
- **_start**: Entry point for Stage2 bootloader.
- **enable_a20**: Enables the A20 line using multiple methods (BIOS, KBC, Fast A20).
- **test_a20**: Verifies if A20 line is successfully enabled.
- **detect_e820**: Detects memory map using BIOS INT 15h AX=0xE820.
- **load_kernel**: Loads the main kernel binary from disk to memory.
- **setup_paging**: Sets up Identity Paging (0-16MB) using 2MB huge pages.
- **setup_idt**: Initializes the Interrupt Descriptor Table (IDT).
- **long_mode_entry**: Entry point for 64-bit Long Mode.
- **exception_common**: Common handler for CPU exceptions.
- **print**: Prints a string (Real Mode).
- **print_hex**: Prints a hex number (Real Mode).

## Kernel Core

### `kernel/kernel_entry.asm`
- **_start**: Kernel entry point (Assembly). Sets up stack, segments, GDT, and calls `kernel_main`.

### `kernel/interrupts.asm`
- **isrX**: ISR stubs for exceptions 0-31.
- **irqX**: IRQ stubs for hardware interrupts 0-15.
- **isr128**: ISR stub for Syscall (INT 0x80).
- **isr_common_stub**: Saves context and calls `isr_exception_handler`.
- **irq_common_stub**: Saves context, calls `irq_common_handler`, and attempts context switch.

### `kernel/kernel.c`
- **print_banner**: Prints the startup banner.
- **print_init**: Prints standard initialization status lines.
- **kernel_main**: Main entry point. Initializes all subsystems (Serial, VGA, IDT, IRQ, Buddy, Keyboard, IPC, Scheduler).
- **kernel_panic**: Handles critical errors, halts interrupts, and attempts soft recovery.

### `kernel/idt.c`
- **pic_remap**: Remaps PIC interrupts to 32-47.
- **idt_load**: Loads IDTR register.
- **idt_set_gate**: Sets an entry in the IDT.
- **idt_init**: Initializes IDT, clears memory, remaps PIC, and installs standard handlers.
- **isr_exception_handler**: High-level C handler for CPU exceptions. Dumps registers and panics.

### `kernel/handlers.c`
- **irq_install_handler**: Registers a custom handler function for an IRQ.
- **irq_uninstall_handler**: Removes a handler.
- **pic_send_eoi**: Sends End-Of-Interrupt signal to PIC(s).
- **irq_common_handler**: Dispatches IRQs to Timer, Keyboard, or registered callbacks.
- **irq_init**: Initializes IRQ system, calls `timer_init`, and unmasks IRQ0/1.

### `kernel/syscall.c`
- **sys_write**: Writes string to VGA.
- **sys_read**: Reads char from keyboard.
- **sys_getpid**: Returns current task PID.
- **sys_uptime**: Returns system uptime in ms.
- **sys_meminfo**: Returns memory stats.
- **sys_yield**: Yields CPU.
- **sys_sleep**: Sleeps for `ms`.
- **sys_exit**: Terminates current process.
- **sys_msgsnd**: Sends IPC message (checked).
- **sys_msgrcv**: Receives IPC message (checked).
- **sys_taskinfo**: Returns task state/priority.
- **syscall_handler**: Main dispatcher switch for INT 0x80.
- **syscall_init**: Initializes syscall subsystem.

### `kernel/permissions.c`
- **perm_init**: Initializes permissions table, grants root to kernel.
- **perm_create_task**: Allocates a permission slot for a new task.
- **perm_destroy_task**: Deactivates a permission slot.
- **perm_grant**: Grants a permission bit to a target task.
- **perm_revoke**: Revokes a permission bit from a target task.
- **perm_check**: Verifies if a task has a specific permission.
- **perm_inherit**: Applies inheritance rules (removes admin bits) to child task.

## Drivers

### `kernel/keyboard.c`
- **keyboard_init**: Initializes state and unmasks IRQ1.
- **keyboard_handler**: Handles IRQ1 scancodes, converts to ASCII using tables.
- **keyboard_getchar**: Blocking char read.
- **keyboard_available**: Checks if buffer has data.
- **keyboard_clear**: Flushes buffer.

### `kernel/vga.c`
- **vga_init**: Clears screen.
- **vga_clear**: Fills screen with blank space.
- **vga_putc**: Put char at cursor, handles newline/scrolling.
- **vga_puts**: Prints string.
- **vga_scroll**: Moves text up 1 line.
- **update_cursor**: Writes to VGA ports to move hardware cursor.

### `kernel/serial.c`
- **serial_init**: Configures COM1 (38400, 8N1).
- **serial_putc**: Waits for THR empty then writes char.
- **serial_puts**: Writes string to serial.

### `kernel/timer.c`
- **timer_init**: Calibrates TSC using PIT, then configures PIT for 1kHz.
- **tsc_calibrate**: Measures CPU frequency against known PIT interval.
- **timer_tick**: Increments tick counter (IRQ0).
- **timer_get_ns/us/ms**: Converts TSC delta to time units.
- **timer_delay_ns/us/ms**: Spin loops using TSC.

### `kernel/vmm/vmm.c`
- **vmm_init**: Bootstraps VMM from bootloader identity map.
- **vmm_create_space**: Allocates a new address space (PML4).
- **vmm_destroy_space**: Frees address space resources.
- **vmm_switch_space**: Loads CR3 with target address space.
- **vmm_flush_tlb_single**: Invalidates TLB for a single page.
- **vmm_flush_tlb_all**: Flushes entire TLB (CR3 reload).

### `kernel/vmm/page_tables.c`
- **vmm_map_page**: Maps a virtual page to a physical frame, allocating intermediate tables.
- **vmm_unmap_page**: Unmaps a page and invalidates TLB.
- **vmm_get_physical**: Walks page tables to resolve virtual address.

## Memory Management

### `kernel/vmm/pmm.c`
- **pmm_init**: Initializes PMM and per-CPU cache.
- **pmm_alloc_page**: Allocates single page, trying lock-free cache first.
- **pmm_alloc**: Generic allocation entry point.
- **pmm_free_page**: Frees single page to local cache.
- **pmm_free**: Generic free entry point.
- **pmm_stats**: Aggregates global and cached stats.

### `kernel/vmm/buddy.c`
- **buddy_init_e820**: Initializes heap from BIOS memory map, reserves secure region.
- **buddy_init**: Core initialization of free lists and initial block.
- **buddy_alloc**: Allocates power-of-2 sized block from free lists.
- **buddy_free**: Returns block and coalesces buddies recursively.
- **secure_alloc**: Allocates from isolated secure region (bump ptr).
- **buddy_stats**: Returns total/used/free stats.

### `kernel/sblock.c`
- **sblock_alloc**: Allocates validated block header + payload.
- **sblock_free**: Decrements ref count, frees if zero.
- **sblock_share**: Increments ref count if sharing allowed.
- **sblock_sign**: Computes CRC32 checksum.
- **sblock_verify**: Validates CRC32 checksum.
- **sblock_access**: Validates permissions and returns data pointer.

### `kernel/scheduler.c`
- **scheduler_init**: Initializes scheduler and creates idle task.
- **task_create**: Creates a standard user task.
- **task_create_full**: Creates task with specific priority and UID. Initializes stack frame.
- **schedule**: Wrapper for yield.
- **yield**: Triggers INT $32.
- **sleep**: Sets task state to SLEEPING and yields.
- **scheduler_switch**: Context switcher. Saves current RSP, picks next task, restores its RSP.
- **exit**: Terminates current task.

## Library and IPC

### `kernel/libx.c`
- **memset/memcpy/memmove/memcmp**: Standard memory ops (optimized).
- **strlen/strcpy/strncpy/strcmp**: Standard string ops.
- **itoa/atoi**: Integer/String conversion.

### `kernel/messages.c`
- **msg_init**: Initializes slab allocator for messages.
- **msg_alloc/free**: Slab-based message allocation.
- **msg_send**: Sends message to target queue (blocking/non-blocking logic handled by caller).
- **msg_receive**: Retrieves message from queue.
- **msg_count**: Returns queue depth.

### `kernel/module.c`
- **module_register**: Registers a module struct.
- **module_load**: Recursively loads dependencies and calls init.
- **modules_init**: Boot-time loader (loads all unloaded modules by priority).
- **device_register**: registers a device node.

### `kernel/shell.c`
- **shell_init**: Clears history and screen.
- **shell_run**: Main loop, reads keyboard, adds to history, executes.
- **shell_execute**: Parses cmd string and dispatches to handler.
- **cmd_***: Individual command handlers (help, mem, tasks, etc).
