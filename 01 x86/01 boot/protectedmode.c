/* arch/x86/boot/protectedmode.c
 * Protected-mode bootstrap for RoosterOS
 *
 * This routine is invoked via a far-jump from realmode.asm. 
 * Upon entry:
 *   EAX = MULTIBOOT bootloader magic
 *   EBX = pointer to multiboot_info_t
 *
 * Responsibilities:
 *   1) Verify bootloader signature
 *   2) Clear BSS
 *   3) Load GDT and reload segment registers
 *   4) Initialize IDT and remap PIC
 *   5) Build and enable paging
 *   6) Enable CPU features (SSE, NX, etc.)
 *   7) Initialize console (VGA & serial)
 *   8) Display boot banner & memory map
 *   9) Call kernel_main(), never return
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>      // for memset
#include "multiboot.h"   // MULTIBOOT_BOOTLOADER_MAGIC, multiboot_info_t
#include "gdt.h"         // gdt_init(), load_gdt(), reload_segments()
#include "idt.h"         // idt_init(), load_idt()
#include "pic.h"         // pic_remap(), pic_disable(), pic_enable()
#include "paging.h"      // paging_init_identity(), paging_enable()
#include "cpu.h"         // cpu_enable_sse(), cpu_enable_nx(), cpu_halt()
#include "console.h"     // console_init(), console_puts(), console_clear(), console_puthex()
#include "kernel.h"      // kernel_main()

/* Symbols defined by the linker script */
extern uint8_t __bss_start;
extern uint8_t __bss_end;

__attribute__((noreturn))
void enter_protected_mode(uint32_t magic, uint32_t mbi_addr) {
    multiboot_info_t *mbi = (multiboot_info_t *)mbi_addr;

    /* 1) Verify Multiboot signature */
    console_init();
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        console_puts("Bootloader magic mismatch: 0x");
        console_puthex(magic);
        console_puts(" — halting.\n");
        cpu_halt();
    }

    /* 2) Clear BSS segment */
    {
        size_t bss_size = (size_t)(&__bss_end - &__bss_start);
        memset(&__bss_start, 0, bss_size);
    }

    /* 3) Set up GDT & reload segments */
    gdt_init();            // build a flat GDT in memory
    load_gdt();            // LGDT [gdtr]
    reload_segments();     // MOV ax,data_sel; MOV ds,ax; etc.

    /* 4) Initialize IDT & PIC */
    pic_remap(0x20, 0x28); // IRQ0–IRQ15 → IDT entries 0x20–0x2F
    pic_disable();         // mask all IRQs until driver ready
    idt_init();            // fill IDT entries with stubs/exceptions
    load_idt();            // LIDT [idtr]

    /* 5) Build and enable paging */
    paging_init_identity(); // identity-map low 1GB
    paging_enable();        // set CR3, CR0.PG

    /* 6) Enable extended CPU features */
    cpu_enable_sse();       // CR4.OSFXSR | CR4.OSXMMEXCPT
    cpu_enable_nx();        // IA32_EFER.NXE via MSR

    /* 7) Initialize console & devices */
    console_clear();
    console_puts("RoosterOS protected mode initialized.\n");
    serial_init(115200);

    /* 8) Display memory map if provided */
    if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
        console_puts("Memory Map:\n");
        print_memory_map(mbi->mmap_addr, mbi->mmap_length);
    }

    /* 9) Transfer control to kernel */
    kernel_main(mbi);

    /* Should never return */
    cpu_halt();
}
