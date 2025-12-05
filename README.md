# NeXs-Operate-System
# x86_64 Kernel & Bootloader

# x86_64 Kernel & Bootloader

A lightweight, purely custom 64-bit Operating System kernel and bootloader from scratch.
It features a custom 2-stage bootloader that transitions from Real Mode to Long Mode, loads a Flat Binary kernel, and provides a dual-output (VGA+Serial) interactive shell with memory management and permission systems.

## Requirements
*   **Linux Environment** (Native or WSL2)
*   `qemu-system-x86_64`
*   `nasm`
*   `gcc` (specifically `x86_64-linux-gnu-gcc` for cross-compilation)
*   `ld`
*   `make` (optional, scripts handle the build)

## How to Build & Run

The build workflow is separated into two clean stages:

### Step 1: Compile the Kernel
This script compiles the C/Assembly sources and generates the pure code binary (`x64kernel.bin`) required by the bootloader.

```bash
cd kernel
./build_kernel.sh
```

### Step 2: Assemble Bootloader & Launch
This script assembles the Stage1/Stage2 bootloaders, creates the disk image, injects the kernel binary, and launches QEMU.

```bash
cd ../boot
./build_and_test.sh
```

*(Note: If you need to rebuild just the bootloader or restart QEMU, you only need to run Step 2).*

## Features
*   **Pure 64-bit Long Mode**: Unrestricted access to 64-bit registers and memory.
*   **Flat Binary Loading**: Kernel is loaded raw at `0x100000` (1MB), avoiding ELF parsing issues.
*   **Dual Console Output**: Logs are printed to both the VGA screen and the Serial Port (visible in your terminal).
*   **Interactive Shell**: A basic command-line interface with commands like `help`, `mem`, `version`, and `perms`.
*   **Buddy Memory Allocator**: Robust memory management with overflow protection.
*   **Interrupt Handling**: Full IDT setup for exception handling.

## Troubleshooting
*   **"Invalid Boot Info"**: Ensure you rebuilt the bootloader (Step 2) after any kernel changes.
*   **No Output**: Check your terminal used for QEMU; serial logs should appear there.
[README.md](https://github.com/user-attachments/files/23972021/README.md)
ry kernel, and provides a dual-output (VGA+Serial) interactive shell with memory management and permission systems.
