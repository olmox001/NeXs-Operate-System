#!/bin/bash
# build_and_test.sh - Bootloader Build & Test Automation
#
# BSD 3-Clause License
# Copyright (c) 2025, NeXs Operate System
#
# Compiles Stage 1 and Stage 2 bootloaders, creates a bootable disk image,
# injects the kernel, and launches QEMU for emulation.

set -e  # Exit on error

# ==============================================================================
# Configuration
# ==============================================================================
STAGE1_SRC="stage1.asm"
STAGE2_SRC="stage2.asm"
STAGE1_BIN="stage1.bin"
STAGE2_BIN="stage2.bin"

DISK_IMG="bootdisk.img"
DISK_SIZE_MB=20
KERNEL_START_SECTOR=64  # Where to write the kernel (must match stage2)

# Kernel Paths
KERNEL_BIN_PATH="../kernel/x64kernel.bin"
KERNEL_ELF_PATH="../kernel/x64kernel.elf"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# ==============================================================================
# Helper Functions
# ==============================================================================

print_step() {
    echo -e "${GREEN}[*]${NC} $1"
}

print_error() {
    echo -e "${RED}[!] ERROR:${NC} $1"
    exit 1
}

check_deps() {
    local missing=0
    for cmd in nasm qemu-system-x86_64 dd; do
        if ! command -v $cmd &> /dev/null; then
            echo -e "${RED}Missing dependency: $cmd${NC}"
            missing=1
        fi
    done
    if [ $missing -eq 1 ]; then
        exit 1
    fi
}

# ==============================================================================
# Build Routines
# ==============================================================================

compile_bootloaders() {
    print_step "Compiling Bootloaders..."
    
    # Stage 1 (MBR)
    nasm -f bin "$STAGE1_SRC" -o "$STAGE1_BIN"
    local s1_size=$(stat -c%s "$STAGE1_BIN")
    if [ "$s1_size" -ne 512 ]; then
        print_error "Stage1 must be exactly 512 bytes (Found: $s1_size)"
    fi
    
    # Stage 2 (Loader)
    nasm -f bin "$STAGE2_SRC" -o "$STAGE2_BIN"
    local s2_size=$(stat -c%s "$STAGE2_BIN")
    local s2_expected=$((32 * 512)) # 16KB
    if [ "$s2_size" -ne "$s2_expected" ]; then
        print_error "Stage2 must be exactly $s2_expected bytes (Found: $s2_size)"
    fi
}

create_image() {
    print_step "Creating Disk Image ($DISK_SIZE_MB MB)..."
    
    # Create empty image
    dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB status=none
    
    # Write Stage 1 (MBR) -> Sector 0
    dd if="$STAGE1_BIN" of="$DISK_IMG" bs=512 count=1 conv=notrunc status=none
    
    # Write Stage 2 -> Sector 1 (32 Sectors)
    dd if="$STAGE2_BIN" of="$DISK_IMG" bs=512 seek=1 count=32 conv=notrunc status=none
    
    # Write Kernel -> Sector 64
    if [ -f "$KERNEL_BIN_PATH" ]; then
        print_step "Injecting Kernel ($KERNEL_BIN_PATH)..."
        dd if="$KERNEL_BIN_PATH" of="$DISK_IMG" bs=512 seek=$KERNEL_START_SECTOR conv=notrunc status=none
    else
        print_error "Kernel binary not found at $KERNEL_BIN_PATH. Build kernel first!"
    fi
    
    local final_size=$(stat -c%s "$DISK_IMG")
    print_step "Bootable Image Ready: $final_size bytes"
}

run_qemu() {
    print_step "Launching QEMU..."
    echo -e "${YELLOW}Press Ctrl+A then X to exit QEMU${NC}"
    
    qemu-system-x86_64 \
        -drive file="$DISK_IMG",format=raw,if=ide \
        -m 256M \
        -cpu qemu64 \
        -no-reboot \
        -serial stdio \
        -vga std \
        -monitor none
}

clean() {
    rm -f *.bin *.img *.lst
    print_step "Cleaned."
}

# ==============================================================================
# Main Logic
# ==============================================================================

check_deps

case "$1" in
    clean)
        clean
        ;;
    build)
        compile_bootloaders
        create_image
        ;;
    test)
        if [ ! -f "$DISK_IMG" ]; then
            compile_bootloaders
            create_image
        fi
        run_qemu
        ;;
    *)
        # Default All-In-One
        compile_bootloaders
        create_image
        run_qemu
        ;;
esac