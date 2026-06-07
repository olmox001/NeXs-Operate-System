# NeXs Operate System 0

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
- **Stage1 (512 bytes)**: MBR bootloader, loads Stage2.
- **Stage2**: A20 gate, E820 memory detection (BIOS INT 15h), paging setup, Long Mode transition.

### Memory Management
- **E820 Detection**: Parses BIOS memory map at boot to detect usable RAM.
- **Buddy Allocator**: Efficient power-of-2 block allocation (4KB to 16MB) with coalescing.
- **Secure Region**: Reserved 64KB region hidden from standard allocator for sensitive keys.
- **Slab Allocator**: Optimized fixed-size allocations for IPC messages (16, 64, 256, 1024, 4096 bytes).
- **Signed Blocks**: Zero-copy specific memory sharing mechanism with CRC32 integrity checks.

### Timing
- **TSC (Time Stamp Counter)**: Calibrated against PIT for high-resolution nanosecond timing.
- **PIT (Programmable Interval Timer)**: Configured for 1000Hz interrupts (Scheduler ticks).
- **High-Resolution Delays**: Blocking API for nanosecond, microsecond, and millisecond delays.

### Multitasking & Security
- **Priority Scheduler**: Preemptive round-robin with priority levels (0-255).
- **Process Model**: PID, UID/GID (Kernel=0, Root=1, User=2), and Task States.
- **Capability System**: Fine-grained permissions (PERM_FILE_READ, PERM_NET_SEND, etc.).
- **IPC**: Asynchronous message passing with support for data and zero-copy pointers.

### Drivers
- **VGA Text Mode**: 80x25 output with color support and hardware cursor control.
- **PS/2 Keyboard**: Interrupt-driven (IRQ1) driver with scancode translation and ring buffer.
- **Serial Port**: COM1 (0x3F8) driver for debug logging/headless operation.
- **Modules**: Linux-like module interface (init/exit hooks, dependencies, priority loading).

## System Calls (INT 0x80)

| Number | Name | Description |
|--------|------|-------------|
| 0 | SYS_READ | Read from input (Keyboard) |
| 1 | SYS_WRITE | Write to output (VGA) |
| 20 | SYS_GETPID | Get current process ID |
| 24 | SYS_YIELD | Yield CPU to scheduler |
| 35 | SYS_SLEEP | Sleep for N milliseconds |
| 60 | SYS_EXIT | Terminate current task |
| 71 | SYS_MSGSND | Send IPC message |
| 72 | SYS_MSGRCV | Check/Receive IPC message |
| 96 | SYS_UPTIME | Get uptime in milliseconds |
| 97 | SYS_MEMINFO | Get memory statistics |
| 98 | SYS_TASKINFO | Get task state/priority |
| 99 | SYS_GETTIME_NS | Get high-res time (ns) |
| 100 | SYS_GETFREQ | Get TSC frequency (Hz) |

## Shell Commands

The built-in kernel shell provides an interactive interface for debugging and management:

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `clear` | Clear screen |
| `echo <text>` | Print text to screen |
| `mem` | Show memory usage (Total, Used, Free) |
| `tasks` | List running tasks (PID, State, Priority, CPU) |
| `pid` | Show current shell PID |
| `uid` | Show current user ID |
| `uptime` | Show system uptime and TSC frequency |
| `sleep <ms>` | Sleep for specified milliseconds |
| `priority <p>` | Set shell process priority (0-255) |
| `perms [id]` | Show task permissions |
| `msg <id>` | Send test IPC message to task ID |
| `version` | Show kernel version and build time |
| `reboot` | Reboot system (Triple Fault) |
| `halt` | Halt CPU |

## Building

### Requirements
- **OS**: Windows (WSL2), Linux, or macOS.
- **Tools**: `nasm`, `x86_64-linux-gnu-gcc` (or equivalent cross-compiler), `qemu-system-x86_64`.

### Installation (Ubuntu/Debian/WSL)
```bash
sudo apt-get update
sudo apt-get install nasm gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu qemu-system-x86
```

### Build & Run
The project uses automated shell scripts for building the kernel and bootloader.

```bash
# 1. Build the Kernel (ELF and Flat Binary)
cd kernel
./build_kernel.sh
# Options: --release (Optimized), --clean (Remove artifacts)

# 2. Build Bootloader, Create Disk Image, and Run QEMU
cd ../boot
./build_and_test.sh
```

### Build Options
- **Debug Build** (Default): `-O2`, includes assertions.
- **Release Build**: `./build_kernel.sh --release` (`-O3`, stripped assertions).

## Project Structure

```
NeXs-OS/
├── boot/
│   ├── stage1.asm          # MBR bootloader (512 bytes)
│   ├── stage2.asm          # Second stage (A20, E820, Paging, Long Mode)
│   └── build_and_test.sh   # Bootloader build & QEMU launcher
├── kernel/
│   ├── kernel_entry.asm    # entry point (Assembly)
│   ├── kernel.c            # Kernel main and initialization
│   ├── kernel.ld           # Linker Script (1MB Load Address)
│   ├── kernel.h            # Globals and Types
│   ├── build_kernel.sh     # Kernel build automation
│   ├── Subsystems:
│   │   ├── buddy.c/h       # Memory Allocator
│   │   ├── scheduler.c     # Process Scheduler
│   │   ├── syscall.c/h     # System Calls
│   │   ├── messages.c/h    # IPC
│   │   ├── module.c/h      # Module System
│   │   ├── shell.c/h       # Interactive Shell
│   │   ├── libx.c/h        # Standard Library (memset, printf, etc.)
│   │   ├── sblock.c/h      # Signed Memory Blocks
│   │   ├── permissions.c/h # Capability System
│   │   ├── idt.c/h         # Interrupt Descriptor Table
│   │   ├── handlers.c/h    # IRQ Handlers
│   │   └── interrupts.asm  # ISR Assembly Stubs
│   └── Drivers:
│       ├── vga.c/h         # Video
│       ├── keyboard.c/h    # Input
│       ├── serial.c/h      # Debug Output
│       └── timer.c/h       # TSC/PIT
└── README.md
```

## Memory Layout

| Address | component | Description |
|---------|-----------|-------------|
| `0x000000` | BIOS/IVT | Legacy Real Mode structures |
| `0x007C00` | Stage1 | MBR Bootloader (512b) |
| `0x007E00` | Stage2 | Second Stage Bootloader (16KB) |
| `0x008570` | Boot Info | E820 Memory Map & Kernel Parameters |
| `0x100000` | **Kernel** | Kernel Code, Data, BSS (Loaded at 1MB) |
| `.......` | Heap | Dynamic Heap (Buddy Allocator) |
| `High Mem`| Secure | Reserved 64KB Secure Region |

## License

BSD 3-Clause License - Copyright (c) 2025, NeXs Operate System
