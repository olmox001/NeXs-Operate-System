#!/bin/bash
# build_kernel.sh - Complete x86_64 Kernel Build System
#
# BSD 3-Clause License
# Copyright (c) 2025, NeXs Operate System
#
# Compiles the C kernel and Assembly stubs, links them into an ELF64 binary,
# and extracts a flat binary for the bootloader.

set -e  # Exit immediately if a command exits with a non-zero status.

# ==============================================================================
# Terminal Colors
# ==============================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ==============================================================================
# Configuration
# ==============================================================================
KERNEL_NAME="x64kernel"
KERNEL_ELF="${KERNEL_NAME}.elf"
KERNEL_BIN="${KERNEL_NAME}.bin"

# Build Mode
OPT_LEVEL="-O2"
BUILD_MODE="Debug"

if [ "$1" == "--release" ]; then
    OPT_LEVEL="-O3 -fomit-frame-pointer"
    BUILD_MODE="Release"
fi

# Toolchain
CC="x86_64-linux-gnu-gcc"
AS="nasm"
LD="x86_64-linux-gnu-ld"

# Compiler Flags
# -mcmodel=large: Support 64-bit absolute addressing
# -mno-red-zone: Disable Red Zone (stack protection) for kernel interupts safety
# -fno-pie: Position Independent Executable disabled (Fixed load address)
CFLAGS="-m64 -ffreestanding -mno-red-zone -mno-sse -mno-sse2 -mno-mmx \
        -mcmodel=large -Wall -Wextra $OPT_LEVEL -fno-pie -fno-stack-protector"

ASFLAGS="-f elf64"
LDFLAGS="-n -T kernel.ld -z max-page-size=0x1000 --no-warn-rwx-segments"

# Source Files Listing
# All new .c files must be added here
C_SOURCES="kernel.c libc.c buddy.c vga.c serial.c keyboard.c idt.c handlers.c timer.c messages.c permissions.c shell.c scheduler.c syscall.c module.c sblock.c"
ASM_SOURCES="kernel_entry.asm interrupts.asm"

# ==============================================================================
# Build Pipeline
# ==============================================================================

echo -e "${CYAN}=== NeXs-OS Kernel Build [${BUILD_MODE}] ===${NC}"
echo ""

# 1. Dependency Check
check_cmd() {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}[ERROR] Required tool '$1' not found.${NC}"
        echo "Install via: sudo apt-get install $2"
        exit 1
    fi
}
check_cmd $CC "gcc-x86-64-linux-gnu"
check_cmd $AS "nasm"
check_cmd $LD "binutils-x86-64-linux-gnu"

# 2. Cleanup
echo -e "${YELLOW}Cleaning artifacts...${NC}"
rm -f *.o ${KERNEL_ELF} ${KERNEL_BIN} kernel.map

# 3. Compile C Sources
echo -e "${YELLOW}Compiling Core...${NC}"
for src in $C_SOURCES; do
    obj="${src%.c}.o"
    echo "  CC $src"
    $CC $CFLAGS -c $src -o $obj
done

# 4. Assemble ASM Sources
echo -e "${YELLOW}Assembling Stubs...${NC}"
for src in $ASM_SOURCES; do
    obj="${src%.asm}.o"
    echo "  AS $src"
    $AS $ASFLAGS $src -o $obj
done

# 5. Link Kernel
echo -e "${YELLOW}Linking Kernel...${NC}"
# IMPORTANT: kernel_entry.o MUST be first to ensure entry point is at the start!
ASM_OBJS="${ASM_SOURCES//.asm/.o}"
C_OBJS="${C_SOURCES//.c/.o}"
OBJ_FILES="kernel_entry.o $C_OBJS ${ASM_OBJS//kernel_entry.o/}"

$LD $LDFLAGS -o $KERNEL_ELF -Map kernel.map $OBJ_FILES
echo -e "${GREEN}[OK] Linked $KERNEL_ELF${NC}"

# 6. Extract Binary (Flat Binary)
echo -e "${YELLOW}Generating Flat Binary...${NC}"
objcopy -O binary $KERNEL_ELF $KERNEL_BIN
KERNEL_SIZE=$(stat -c%s $KERNEL_BIN)
echo -e "${GREEN}[OK] Generated $KERNEL_BIN ($KERNEL_SIZE bytes)${NC}"

# 7. Analysis (Optional)
ENTRY_POINT=$(readelf -h $KERNEL_ELF | grep "Entry point" | awk '{print $4}')
echo ""
echo -e "  Entry Point: ${CYAN}$ENTRY_POINT${NC}"
echo -e "  Sections:"
readelf -S $KERNEL_ELF | grep -E " \.text| \.data| \.bss" | awk '{printf "    %-15s %-10s %-10s\n", $2, $4, $5}'

echo ""
echo -e "${GREEN}=== Build Success ===${NC}"
echo "Run 'cd ../boot && ./build_and_test.sh' to deploy."