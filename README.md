# NeXs Operate System

A minimal, educational 64-bit operating system kernel written from scratch in Assembly and C.

## Overview

NeXs-OS is a bare-metal operating system that boots from BIOS, transitions to 64-bit Long Mode, and provides a fully functional multitasking environment with a shell interface. The project demonstrates OS fundamentals including bootloading, memory management, scheduling, and system calls.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Shell Interface                          │
├─────────────────────────────────────────────────────────────┤
│  System Calls │  IPC Messages  │  Permissions  │  Modules   │
├───────────────┴────────────────┴───────────────┴────────────┤
│         Priority Scheduler (Mach/L4 inspired)                │
├─────────────────────────────────────────────────────────────┤
│  TSC Timer  │  Buddy Allocator  │  Slab Allocator           │
├─────────────┴───────────────────┴───────────────────────────┤
│       IDT/IRQ       │      VGA/Serial      │    Keyboard    │
├─────────────────────┴──────────────────────┴────────────────┤
│                   x86_64 Long Mode (Ring 0)                  │
├─────────────────────────────────────────────────────────────┤
│           Stage1 (MBR) → Stage2 (Protected → Long)          │
└─────────────────────────────────────────────────────────────┘
```

## Features

### Bootloader
- **Stage1 (512 bytes)**: MBR bootloader, loads Stage2
- **Stage2**: A20 gate, E820 memory detection, paging, Long Mode transition

### Memory Management
- **E820 Detection**: BIOS memory map at boot
- **Buddy Allocator**: Power-of-2 block allocation (4KB-16MB)
- **Secure Region**: 64KB hidden from normal allocator (for future encryption keys)
- **Slab Allocator**: Variable-size message buffers (16/64/256/1024/4096 bytes)

### Timing
- **TSC (Time Stamp Counter)**: CPU clock precision (~1ns)
- **PIT Calibration**: Measures TSC frequency at boot
- **High-Resolution Delays**: `timer_delay_ns()`, `timer_delay_us()`, `timer_delay_ms()`

### Multitasking
- **Priority Scheduler**: 256 priority levels (0=highest)
- **UID System**: Kernel (0), Root (1), User (2) privileges
- **Task States**: READY, RUNNING, SLEEPING, WAITING, DEAD
- **Preemption**: Via PIT IRQ0 at 1000Hz

### IPC & Security
- **Signed Blocks**: CRC32-signed zero-copy memory sharing
- **Capability Permissions**: Fine-grained per-task access control
- **Message Queue**: Dynamic allocation per-task

### Drivers
- **VGA Text Mode**: 80x25, optimized 64-bit scroll
- **PS/2 Keyboard**: Scancode set 1, full key handling
- **Serial Port**: UART 16550A (COM1, 38400 baud)
- **Module System**: Linux-like `module_info` interface

## System Calls (INT 0x80)

| Number | Name | Description |
|--------|------|-------------|
| 0 | SYS_READ | Read from file descriptor |
| 1 | SYS_WRITE | Write to stdout |
| 20 | SYS_GETPID | Get current process ID |
| 24 | SYS_YIELD | Yield CPU to scheduler |
| 35 | SYS_SLEEP | Sleep for N milliseconds |
| 60 | SYS_EXIT | Terminate current task |
| 71 | SYS_MSGSND | Send IPC message |
| 72 | SYS_MSGRCV | Receive IPC message |
| 96 | SYS_UPTIME | Get uptime in milliseconds |
| 97 | SYS_MEMINFO | Get memory statistics |
| 98 | SYS_TASKINFO | Get task information |
| 99 | SYS_GETTIME_NS | Get time in nanoseconds |
| 100 | SYS_GETFREQ | Get TSC frequency (Hz) |

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `clear` | Clear screen |
| `echo <msg>` | Print message |
| `mem` | Show memory statistics |
| `tasks` | List running tasks |
| `pid` | Show current process ID |
| `uid` | Show current user ID |
| `uptime` | Show system uptime (TSC MHz) |
| `sleep <ms>` | Sleep for milliseconds |
| `priority <p>` | Set shell priority (0-255) |
| `perms [id]` | Show task permissions |
| `msg <id>` | Send test message to task |
| `version` | Show kernel version |
| `reboot` | Reboot system |
| `halt` | Halt CPU |

## Building

### Requirements
- WSL2 or Linux
- `nasm` (assembler)
- `x86_64-linux-gnu-gcc` (cross-compiler)
- `qemu-system-x86_64` (emulator)

### Installation (Ubuntu/Debian)
```bash
sudo apt-get install nasm gcc-x86-64-linux-gnu qemu-system-x86
```

### Build & Run
```bash
# Build kernel
cd kernel
./build_kernel.sh

# Build bootloader and test
cd ../boot
./build_and_test.sh
```

### Build Options
```bash
./build_kernel.sh --release  # Optimized build
./build_kernel.sh --debug    # Debug symbols
./build_kernel.sh --clean    # Clean only
```

## Project Structure

```
os_bad/
├── boot/
│   ├── stage1.asm          # MBR bootloader (512 bytes)
│   ├── stage2.asm          # Long Mode loader, E820
│   └── build_and_test.sh   # Build and QEMU test script
├── kernel/
│   ├── kernel_entry.asm    # Kernel entry point
│   ├── interrupts.asm      # ISR/IRQ stubs
│   ├── kernel.c            # Main kernel
│   ├── kernel.h            # Core types and definitions
│   ├── kernel.ld           # Linker script
│   ├── buddy.c/h           # Buddy allocator
│   ├── timer.c/h           # TSC high-precision timer
│   ├── scheduler.c         # Priority scheduler
│   ├── process.h           # Task structures
│   ├── syscall.c/h         # System call dispatcher
│   ├── messages.c/h        # IPC slab allocator
│   ├── sblock.c/h          # Signed memory blocks
│   ├── permissions.c/h     # Capability system
│   ├── module.c/h          # Driver module system
│   ├── shell.c/h           # Interactive shell
│   ├── vga.c/h             # VGA text driver
│   ├── keyboard.c/h        # PS/2 keyboard driver
│   ├── serial.c/h          # Serial port driver
│   ├── idt.c/h             # IDT setup
│   ├── handlers.c/h        # IRQ handlers
│   ├── libc.c/h            # Minimal C library
│   └── build_kernel.sh     # Build script
├── README.md
└── ROADMAP.md
```

## Memory Layout

```
+---------------------------+ 128MB+ (depends on RAM)
|   Secure Key Storage      | 64KB (hidden from buddy)
+---------------------------+
|   Dynamic Heap            | Managed by buddy allocator
+---------------------------+ ~2MB
|   Kernel BSS/Data         |
+---------------------------+ ~1.5MB
|   Kernel Code             | ~48KB
+---------------------------+ 1MB
|   Boot Info / E820 Map    |
+---------------------------+ 0x7E00
|   Stage2 Bootloader       | 16KB
+---------------------------+ 0x7C00
|   Stage1 (MBR)            | 512 bytes
+---------------------------+ 0
```

## License

BSD 3-Clause License - Copyright (c) 2025, NeXs Operate System
