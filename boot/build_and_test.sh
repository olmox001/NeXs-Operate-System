#!/bin/bash

# ==============================================================================
# Script di Compilazione e Test Bootloader
# ==============================================================================
# Compila stage1, stage2, crea immagine disco e testa con QEMU
# ==============================================================================

set -e  # Exit on error

# Colori per output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configurazione
STAGE1_SRC="stage1.asm"
STAGE2_SRC="stage2.asm"
STAGE1_BIN="stage1.bin"
STAGE2_BIN="stage2.bin"
DISK_IMG="bootdisk.img"
DISK_SIZE_MB=20
KERNEL_START_SECTOR=64

# ==============================================================================
# Funzioni Utility
# ==============================================================================

print_header() {
    echo -e "${BLUE}=================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}=================================================${NC}"
}

print_step() {
    echo -e "${GREEN}[*]${NC} $1"
}

print_error() {
    echo -e "${RED}[!] ERRORE:${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!] ATTENZIONE:${NC} $1"
}

check_dependencies() {
    print_step "Verifica dipendenze..."
    
    local missing_deps=()
    
    if ! command -v nasm &> /dev/null; then
        missing_deps+=("nasm")
    fi
    
    if ! command -v qemu-system-x86_64 &> /dev/null && ! command -v qemu-system-i386 &> /dev/null; then
        missing_deps+=("qemu-system-x86_64 o qemu-system-i386")
    fi
    
    if ! command -v dd &> /dev/null; then
        missing_deps+=("dd")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Dipendenze mancanti: ${missing_deps[*]}"
        echo "Installa con: sudo apt-get install nasm qemu-system-x86 (Ubuntu/Debian)"
        echo "             sudo dnf install nasm qemu-system-x86 (Fedora)"
        exit 1
    fi
    
    print_step "Tutte le dipendenze presenti"
}

# ==============================================================================
# Compilazione
# ==============================================================================

compile_stage1() {
    print_step "Compilazione Stage1..."
    
    if [ ! -f "$STAGE1_SRC" ]; then
        print_error "File sorgente $STAGE1_SRC non trovato"
        exit 1
    fi
    
    nasm -f bin "$STAGE1_SRC" -o "$STAGE1_BIN" -l stage1.lst
    
    if [ $? -ne 0 ]; then
        print_error "Compilazione Stage1 fallita"
        exit 1
    fi
    
    # Verifica dimensione esatta 512 byte
    local size=$(stat -c%s "$STAGE1_BIN" 2>/dev/null || stat -f%z "$STAGE1_BIN" 2>/dev/null)
    
    if [ "$size" -ne 512 ]; then
        print_error "Stage1 deve essere esattamente 512 byte, trovati: $size byte"
        exit 1
    fi
    
    # Verifica boot signature
    local signature=$(xxd -p -s 510 -l 2 "$STAGE1_BIN")
    if [ "$signature" != "55aa" ]; then
        print_error "Boot signature invalida: 0x$signature (attesa: 0x55aa)"
        exit 1
    fi
    
    print_step "Stage1 compilato: $size byte, signature: 0x$signature"
}

compile_stage2() {
    print_step "Compilazione Stage2 (x86_64 Long Mode)..."
    
    if [ ! -f "$STAGE2_SRC" ]; then
        print_error "File sorgente $STAGE2_SRC non trovato"
        exit 1
    fi
    
    nasm -f bin "$STAGE2_SRC" -o "$STAGE2_BIN" -l stage2.lst
    
    if [ $? -ne 0 ]; then
        print_error "Compilazione Stage2 fallita"
        exit 1
    fi
    
    # Verifica dimensione esatta 16KB (32 settori)
    local size=$(stat -c%s "$STAGE2_BIN" 2>/dev/null || stat -f%z "$STAGE2_BIN" 2>/dev/null)
    local expected=$((32 * 512))
    
    if [ "$size" -ne "$expected" ]; then
        print_error "Stage2 deve essere esattamente $expected byte (32 settori), trovati: $size byte"
        exit 1
    fi
    
    # Verifica magic signature all'inizio
    local signature=$(xxd -p -l 2 "$STAGE2_BIN")
    if [ "$signature" != "55aa" ]; then
        print_error "Stage2 magic signature invalida: 0x$signature (attesa: 0xaa55)"
        exit 1
    fi
    
    print_step "Stage2 compilato: $size byte (long mode), magic: 0x$signature"
}

# ==============================================================================
# Creazione Immagine Disco
# ==============================================================================

create_disk_image() {
    print_step "Creazione immagine disco ($DISK_SIZE_MB MB)..."
    
    # Crea immagine vuota
    dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB status=none
    
    if [ $? -ne 0 ]; then
        print_error "Creazione immagine disco fallita"
        exit 1
    fi
    
    # Scrivi stage1 nel primo settore (MBR)
    print_step "Scrittura Stage1 nel settore 0 (MBR)..."
    dd if="$STAGE1_BIN" of="$DISK_IMG" bs=512 count=1 conv=notrunc status=none
    
    # Scrivi stage2 a partire dal settore 1 (32 settori)
    print_step "Scrittura Stage2 nei settori 1-32..."
    dd if="$STAGE2_BIN" of="$DISK_IMG" bs=512 seek=1 count=32 conv=notrunc status=none
    
    # Cerca kernel ELF64 (Priorità 1: locale, Priorità 2: esterno)
    local KERNEL_FOUND=0
    # Priorità al Flat Binary (.bin)
    if [ -f "../kernel/x64kernel.bin" ]; then
        print_step "Usando kernel esterno: ../kernel/x64kernel.bin"
        dd if="../kernel/x64kernel.bin" of="$DISK_IMG" bs=512 seek=$KERNEL_START_SECTOR conv=notrunc status=none
        KERNEL_FOUND=1
    elif [ -f "../kernel/x64kernel.elf" ]; then
        print_warning "Usando ELF (Potrebbe fallire): x64kernel.elf"
        # Fallback: try to strip it on the fly if bin missing?
        # For now just warn
        dd if="../kernel/x64kernel.elf" of="$DISK_IMG" bs=512 seek=$KERNEL_START_SECTOR conv=notrunc status=none
        KERNEL_FOUND=1
    fi

    if [ $KERNEL_FOUND -eq 0 ]; then
        print_error "Nessun kernel trovato (x64kernel.elf o ../kernel/kernel.bin)"
        echo "Costruisci il kernel prima di eseguire questo script."
        rm -f "$DISK_IMG"
        exit 1
    fi
    
    local img_size=$(stat -c%s "$DISK_IMG" 2>/dev/null || stat -f%z "$DISK_IMG" 2>/dev/null)
    print_step "Immagine disco creata: $img_size byte"
}

# ==============================================================================
# Test con QEMU
# ==============================================================================

test_qemu() {
    print_header "Avvio test QEMU"
    
    print_step "Parametri test:"
    echo "  - Memoria: 128MB"
    echo "  - CPU: 1 core"
    echo "  - Display: VGA standard"
    echo "  - Serial: output su stdio"
    echo ""
    print_warning "Premi Ctrl+A poi X per uscire da QEMU"
    echo ""
    
    sleep 2
    
    # Seleziona QEMU disponibile
    local QEMU=""
    if command -v qemu-system-x86_64 &> /dev/null; then
        QEMU="qemu-system-x86_64"
    elif command -v qemu-system-i386 &> /dev/null; then
        QEMU="qemu-system-i386"
    else
        print_error "QEMU non trovato"
        exit 1
    fi
    
    # Avvia QEMU (Configurazione Stabile per UEFI/Long Mode)
    $QEMU \
        -drive file="$DISK_IMG",format=raw,if=ide \
        -m 256M \
        -machine q35,smm=off \
        -cpu qemu64 \
        -accel kvm \
        -no-reboot \
        -d int,cpu_reset \
        -D qemu.log \
        -serial stdio \
        -vga std
    print_step "Test QEMU completato"
}

# ==============================================================================
# Pulizia
# ==============================================================================

clean() {
    print_step "Pulizia file temporanei..."
    rm -f "$STAGE1_BIN" "$STAGE2_BIN" "$DISK_IMG"
    rm -f stage1.lst stage2.lst
    rm -f dummy_kernel.bin dummy_kernel.asm
    print_step "Pulizia completata"
}

# ==============================================================================
# Main
# ==============================================================================

main() {
    print_header "BUILD E TEST BOOTLOADER"
    
    # Parse argomenti
    case "${1:-}" in
        clean)
            clean
            exit 0
            ;;
        build)
            check_dependencies
            compile_stage1
            compile_stage2
            create_disk_image
            print_header "BUILD COMPLETATO CON SUCCESSO"
            exit 0
            ;;
        test)
            if [ ! -f "$DISK_IMG" ]; then
                print_error "Immagine disco non trovata. Esegui prima: $0 build"
                exit 1
            fi
            test_qemu
            exit 0
            ;;
        "")
            # Default: build e test
            check_dependencies
            compile_stage1
            compile_stage2
            create_disk_image
            print_header "BUILD COMPLETATO - AVVIO TEST"
            test_qemu
            ;;
        *)
            echo "Uso: $0 [build|test|clean]"
            echo ""
            echo "Comandi:"
            echo "  build  - Compila bootloader e crea immagine disco"
            echo "  test   - Testa immagine esistente con QEMU"
            echo "  clean  - Rimuove file generati"
            echo "  (vuoto)- Build e test automatico"
            exit 1
            ;;
    esac
}

main "$@"