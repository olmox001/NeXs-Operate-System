#!/bin/bash
# build_kernel.sh - Complete x86_64 Kernel Build and Test Script

set -e  # Exit on error

# Colors
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

# Compiler settings
CC="x86_64-linux-gnu-gcc"
AS="nasm"
LD="x86_64-linux-gnu-ld"

CFLAGS="-m64 -ffreestanding -mno-red-zone -mno-sse -mno-sse2 -mno-mmx \
        -mcmodel=large -Wall -Wextra -O2 -fno-pie -fno-stack-protector"
ASFLAGS="-f elf64"
LDFLAGS="-n -T kernel.ld -z max-page-size=0x1000 --no-warn-rwx-segments"

# Source files
C_SOURCES="kernel.c libc.c buddy.c vga.c serial.c keyboard.c idt.c handlers.c messages.c permissions.c shell.c"
ASM_SOURCES="kernel_entry.asm interrupts.asm"

echo -e "${CYAN}=== x86_64 Kernel Build System ===${NC}"
echo ""

# Check dependencies
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

# Clean previous build
echo -e "${YELLOW}Cleaning previous build...${NC}"
rm -f *.o ${KERNEL_ELF} ${KERNEL_BIN}
echo -e "${GREEN}[✓]${NC} Clean complete"
echo ""

# Compile C sources
echo -e "${YELLOW}Compiling C sources...${NC}"
for src in $C_SOURCES; do
    obj="${src%.c}.o"
    echo "  Compiling $src -> $obj"
    $CC $CFLAGS -c $src -o $obj
done
echo -e "${GREEN}[✓]${NC} C compilation complete"
echo ""

# Compile assembly sources
echo -e "${YELLOW}Assembling ASM sources...${NC}"
for src in $ASM_SOURCES; do
    obj="${src%.asm}.o"
    echo "  Assembling $src -> $obj"
    $AS $ASFLAGS $src -o $obj
done
echo -e "${GREEN}[✓]${NC} Assembly complete"
echo ""

# Link kernel
echo -e "${YELLOW}Linking kernel...${NC}"
# IMPORTANT: kernel_entry.o MUST be first!
OBJ_FILES="kernel_entry.o ${C_SOURCES//.c/.o} interrupts.o"
$LD $LDFLAGS -o $KERNEL_ELF -Map kernel.map $OBJ_FILES
echo -e "${GREEN}[✓]${NC} Linking complete"
echo ""

# Binary extraction (Required for Flat Binary Loader)
echo -e "${YELLOW}Extracting binary...${NC}"
objcopy -O binary $KERNEL_ELF $KERNEL_BIN
KERNEL_SIZE=$(stat -c%s $KERNEL_BIN)
echo "  Kernel size: $KERNEL_SIZE bytes"
echo -e "${GREEN}[✓]${NC} Binary extraction complete"
echo ""

# Note: Installation to disk is handled by boot/build_and_test.sh

# ELF Debug Analysis
echo -e "${YELLOW}Analyzing Kernel ELF...${NC}"
ENTRY_POINT=$(readelf -h $KERNEL_ELF | grep "Entry point" | awk '{print $4}')
echo -e "  Entry Point: ${CYAN}$ENTRY_POINT${NC}"

echo -e "  ${YELLOW}Sections:${NC}"
readelf -S $KERNEL_ELF | grep -E " \.text| \.data| \.rodata| \.bss| \.note\.GNU-stack" | awk '{printf "    %-15s %-10s %-10s\n", $2, $4, $5}'

echo -e "  ${YELLOW}Segments (PHDRS):${NC}"
readelf -l $KERNEL_ELF | grep -E "LOAD|Entry" | grep -v "Requesting"

# Check for Executable Stack
if readelf -l $KERNEL_ELF | grep -q "GNU_STACK"; then
    STACK_FLAGS=$(readelf -l $KERNEL_ELF | grep "GNU_STACK" | awk '{print $7}')
    if [[ "$STACK_FLAGS" == "RWE" ]]; then
        echo -e "${RED}[WARNING] Stack is EXECUTABLE (RWX)!${NC}"
    else
        echo -e "${GREEN}[✓]${NC} Stack is Non-Executable ($STACK_FLAGS)"
    fi
else
    # If using sections instead of segments for stack note
    if readelf -S $KERNEL_ELF | grep -q ".note.GNU-stack"; then
        echo -e "${GREEN}[✓]${NC} Found .note.GNU-stack section"
    else
        echo -e "${YELLOW}[?] No GNU_STACK header found (Check legacy warnings)${NC}"
    fi
fi
echo ""

# Display info
echo -e "${CYAN}=== Build Summary ===${NC}"
echo "  Kernel ELF: $KERNEL_ELF"
echo "  Kernel BIN: $KERNEL_BIN"
echo "  Size: $KERNEL_SIZE bytes"
echo "  Boot disk: $BOOT_DISK"
echo ""

# Test guidance
echo -e "${YELLOW}To test, run:${NC}"
echo "  cd ../boot && ./build_and_test.sh"

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"