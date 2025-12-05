// kernel.c - x86_64 Kernel Main Entry Point
#include "kernel.h"
#include "vga.h"
#include "serial.h"
#include "libc.h"
#include "buddy.h"
#include "idt.h"
#include "keyboard.h"
#include "messages.h"
#include "permissions.h"
#include "shell.h"

// External IRQ initialization (defined in handlers.c or interrupts.asm)
void irq_init(void);

// Print startup banner
static void print_banner(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("\n");
    vga_puts("  ========================================\n");
    vga_puts("   x86_64 Kernel ");
    vga_puts(KERNEL_VERSION);
    vga_puts("\n");
    vga_puts("  ========================================\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

// Print initialization step
static void print_init(const char* component, bool success) {
    vga_puts("  [");
    if (success) {
        vga_set_color(VGA_GREEN, VGA_BLACK);
        vga_puts(" OK ");
    } else {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("FAIL");
    }
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("] ");
    vga_puts(component);
    vga_putc('\n');
}

// Kernel main entry point
void kernel_main(struct boot_info* info) {
    // Initialize Serial FIRST for debug logging (Headless support)
    serial_init();
    // Initialize VGA (now mirrors to serial)
    vga_init();
    
    // Dual log
    vga_puts("DEBUG: Entered kernel_main\n");
    
    // Validate boot info
    if (info == NULL) {
        vga_puts("WARNING: info is NULL (Bootloader Issue?)\n");
    } else {
        vga_puts("Info Ptr: "); vga_putx((uint64_t)info); vga_puts("\n");
        vga_puts("Magic: "); vga_putx(info->magic); vga_puts("\n");
        vga_puts("Expected: "); vga_putx(0xDEADBEEF); vga_puts("\n");
    }

    if (!info || info->magic != 0xDEADBEEF) {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("ERROR: Invalid boot info magic (soft pass)\n");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    } else {
        vga_puts("Boot Info OK\n");
    }
    
    // Print banner
    print_banner();
    
    // DEBUG: Direct write to video memory (Green 'X' at column 40)
    *((volatile uint16_t*)0xB8050) = 0x2F58; // 2F = White on Green, 58 = 'X'
    
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Initializing kernel subsystems...\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    // Initialize IDT
    vga_puts("DEBUG: Init IDT...\n");
    idt_init();
    print_init("Interrupt Descriptor Table", true);
    
    // Initialize IRQ handlers
    vga_puts("DEBUG: Init IRQ...\n");
    irq_init();
    print_init("IRQ Handlers", true);
    
    // Initialize buddy allocator
    vga_puts("DEBUG: Init Buddy...\n");
    
    // Dynamic Heap Setup: Use memory between kernel end and stack (2MB)
    extern uint64_t _kernel_end;
    // Page align up (assuming 4K pages)
    uint64_t heap_start_addr = ((uint64_t)&_kernel_end + 4095) & ~4095;
    
    // Allow 512KB heap
    buddy_init((void*)heap_start_addr, 0x80000); 
    
    print_init("Memory Allocator (Buddy)", true);
    
    // Show memory statistics
    size_t total, used, free_mem;
    buddy_stats(&total, &used, &free_mem);
    vga_puts("      Heap: ");
    vga_puti(total / 1024);
    vga_puts(" KB at ");
    vga_putx(heap_start_addr);
    vga_putc('\n');
    
    // Initialize keyboard
    vga_puts("DEBUG: Init Keyboard...\n");
    keyboard_init();
    print_init("PS/2 Keyboard Driver", true);
    
    // Initialize message system
    msg_init();
    print_init("IPC Message System", true);
    
    // Initialize permission system
    perm_init();
    print_init("Capability System (Royalty)", true);
    vga_puts("      Task 0 (kernel): All permissions\n");
    
    // Create test task with permissions
    int result = perm_create_task(1, 0, 
        PERM_MEMORY_ALLOC | PERM_MEMORY_FREE | 
        PERM_MSG_SEND | PERM_MSG_RECEIVE |
        PERM_SHELL_ACCESS);
    if (result == 0) {
        vga_puts("      Task 1 created with user permissions\n");
    }
    
    vga_putc('\n');
    vga_set_color(VGA_GREEN, VGA_BLACK);
    vga_puts("==> Kernel initialization complete!\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    // Test memory (optional, keeping for validation)
    /*
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("Testing memory allocator...\n");
    void* test_ptr = buddy_alloc(1024);
    if (test_ptr) {
        vga_puts("  Allocated 1KB at ");
        vga_putx((uint64_t)test_ptr);
        vga_putc('\n');
        buddy_free(test_ptr);
        vga_puts("  Freed successfully\n");
    }
    vga_putc('\n');
    vga_set_color(VGA_WHITE, VGA_BLACK);
    */
    
    // Initialize and run shell
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Enabling Interrupts...\n");
    sti();
    
    vga_puts("Starting Shell...\n");
    shell_init();
    
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Ready.\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    // Enter shell loop (never returns)
    shell_run();
    
    // Should never reach here
    PANIC("Kernel main returned!");
}

// Global Panic Handler
void kernel_panic(const char* message, const char* file, int line) {
    // Disable interrupts to ensure atomic output
    cli();

    vga_set_color(VGA_WHITE, VGA_RED);
    vga_puts("\n\n!! KERNEL PANIC !!\n");
    vga_puts("Reason: ");
    vga_puts(message);
    vga_puts("\nFile:   ");
    vga_puts(file);
    vga_puts("\nLine:   ");
    vga_puti(line);
    vga_puts("\n\n");
    
    vga_puts("Attempting soft recovery...\n");
    
    // Simple delay loop
    for(volatile int i = 0; i < 10000000; i++); 

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Restarting Shell...\n");
    
    // Enable interrupts
    sti();
    
    // Soft Reset
    shell_init();
    shell_run();
    
    // Hard halt if recovery fails
    vga_set_color(VGA_RED, VGA_BLACK);
    vga_puts("System Halted (Recovery Failed).");
    while(1) hlt();
}