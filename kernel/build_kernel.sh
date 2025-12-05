#!/bin/bash
# build_kernel.sh - Complete x86_64 Kernel Build and Test Script
#
# BSD 3-Clause License
#
# Copyright (c) 2025, NeXs Operate System
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -e  # Exit on error

# Terminal Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
KERNEL_NAME="x64kernel"
BOOT_DISK="bootdisk.img"
KERNEL_ELF="${KERNEL_NAME}.elf"
KERNEL_BIN="${KERNEL_NAME}.bin"

# Build Mode (Default vs Optimized)
# Default: -O2 (Standard)
# Release: -O3 (Max Optimization)
OPT_LEVEL="-O2"
BUILD_MODE="Standard"

if [ "$1" == "--release" ]; then
    OPT_LEVEL="-O3 -fomit-frame-pointer"
    BUILD_MODE="Release (Optimized)"
fi

# Toolchain Settings
CC="x86_64-linux-gnu-gcc"
AS="nasm"
LD="x86_64-linux-gnu-ld"

# Compiler Flags
# -mcmodel=large: Support 64-bit absolute addressing
# -mno-red-zone: Disable Red Zone (stack protection) for kernel space
CFLAGS="-m64 -ffreestanding -mno-red-zone -mno-sse -mno-sse2 -mno-mmx \
        -mcmodel=large -Wall -Wextra $OPT_LEVEL -fno-pie -fno-stack-protector"
ASFLAGS="-f elf64"
LDFLAGS="-n -T kernel.ld -z max-page-size=0x1000 --no-warn-rwx-segments"

# Source Files
C_SOURCES="kernel.c libc.c buddy.c vga.c serial.c keyboard.c idt.c handlers.c messages.c permissions.c shell.c"
ASM_SOURCES="kernel_entry.asm interrupts.asm"

echo -e "${CYAN}=== x86_64 Kernel Build System [${BUILD_MODE}] ===${NC}"
echo ""

# Dependency Check
check_dependency() {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}[ERROR] $1 is not installed${NC}"
        echo "Install with: sudo apt-get install $2"
        exit 1
    fi
    echo -e "${GREEN}[✓]${NC} Found $1"
}

echo -e "${YELLOW}Checking dependencies...${NC}"
check_dependency x86_64-linux-gnu-gcc "gcc-x86-64-linux-gnu"
check_dependency nasm "nasm"
check_dependency qemu-system-x86_64 "qemu-system-x86"
echo ""

# Clean Artifacts
echo -e "${YELLOW}Cleaning previous build...${NC}"
rm -f *.o ${KERNEL_ELF} ${KERNEL_BIN}
echo -e "${GREEN}[✓]${NC} Clean complete"
echo ""

# Compile C Sources
echo -e "${YELLOW}Compiling C sources...${NC}"
for src in $C_SOURCES; do
    obj="${src%.c}.o"
    echo "  Compiling $src -> $obj"
    $CC $CFLAGS -c $src -o $obj
done
echo -e "${GREEN}[✓]${NC} C compilation complete"
echo ""

# Assemble ASM Sources
echo -e "${YELLOW}Assembling ASM sources...${NC}"
for src in $ASM_SOURCES; do
    obj="${src%.asm}.o"
    echo "  Assembling $src -> $obj"
    $AS $ASFLAGS $src -o $obj
done
echo -e "${GREEN}[✓]${NC} Assembly complete"
echo ""

# Link Kernel
echo -e "${YELLOW}Linking kernel...${NC}"
# IMPORTANT: kernel_entry.o MUST be linked first to ensure entry point is at the start!
OBJ_FILES="kernel_entry.o ${C_SOURCES//.c/.o} interrupts.o"
$LD $LDFLAGS -o $KERNEL_ELF -Map kernel.map $OBJ_FILES
echo -e "${GREEN}[✓]${NC} Linking complete"
echo ""

# Extract Binary (Objcopy)
# Required because the bootloader loads a raw Flat Binary, not an ELF
echo -e "${YELLOW}Extracting binary...${NC}"
objcopy -O binary $KERNEL_ELF $KERNEL_BIN
KERNEL_SIZE=$(stat -c%s $KERNEL_BIN)
echo "  Kernel size: $KERNEL_SIZE bytes"
echo -e "${GREEN}[✓]${NC} Binary extraction complete"
echo ""

# Note: Disk installation is handled by ../boot/build_and_test.sh

# ELF Debug Analysis
echo -e "${YELLOW}Analyzing Kernel ELF...${NC}"
ENTRY_POINT=$(readelf -h $KERNEL_ELF | grep "Entry point" | awk '{print $4}')
echo -e "  Entry Point: ${CYAN}$ENTRY_POINT${NC}"

echo -e "  ${YELLOW}Sections:${NC}"
readelf -S $KERNEL_ELF | grep -E " \.text| \.data| \.rodata| \.bss| \.note\.GNU-stack" | awk '{printf "    %-15s %-10s %-10s\n", $2, $4, $5}'

echo -e "  ${YELLOW}Segments (PHDRS):${NC}"
readelf -l $KERNEL_ELF | grep -E "LOAD|Entry" | grep -v "Requesting"

# Check for Executable Stack (Security Warning)
if readelf -l $KERNEL_ELF | grep -q "GNU_STACK"; then
    STACK_FLAGS=$(readelf -l $KERNEL_ELF | grep "GNU_STACK" | awk '{print $7}')
    if [[ "$STACK_FLAGS" == "RWE" ]]; then
        echo -e "${RED}[WARNING] Stack is EXECUTABLE (RWX)!${NC}"
    else
        echo -e "${GREEN}[✓]${NC} Stack is Non-Executable ($STACK_FLAGS)"
    fi
else
    # Fallback to section check
    if readelf -S $KERNEL_ELF | grep -q ".note.GNU-stack"; then
        echo -e "${GREEN}[✓]${NC} Found .note.GNU-stack section"
    else
        echo -e "${YELLOW}[?] No GNU_STACK header found (Check legacy warnings)${NC}"
    fi
fi
echo ""

# Summary
echo -e "${CYAN}=== Build Summary ===${NC}"
echo "  Kernel ELF: $KERNEL_ELF"
echo "  Kernel BIN: $KERNEL_BIN"
echo "  Size: $KERNEL_SIZE bytes"
echo "  Boot disk: $BOOT_DISK"
echo "  Mode: $BUILD_MODE"
echo ""

# Instructions
echo -e "${YELLOW}To test, run:${NC}"
echo "  cd ../boot && ./build_and_test.sh"

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"