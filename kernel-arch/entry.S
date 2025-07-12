/*
 * arch/x86/kernel-arch/entry.S
 * Ultra–advanced 32-bit flat kernel entry (Multiboot + GDT + IDT + Paging + PIC)
 */

/* Multiboot header so GRUB or any Multiboot-compliant loader can boot us */
.section .multiboot
  .align 4
  .long 0x1BADB002                /* magic */
  .long 0x00010003                /* flags: HAVE_MEM_INFO + VIDEO */
  .long -(0x1BADB002 + 0x00010003)/* checksum */

/* TEXT segment begins here */
.section .text
  .align 16
  .globl _start
  .type  _start, @function
_start:
  cli                             /* disable interrupts */

  /* 1) Load our GDT */
  lgdt [gdtr]
  mov  $0x10, %ax                 /* data segment selector */
  mov  %ax, %ds
  mov  %ax, %es
  mov  %ax, %fs
  mov  %ax, %gs
  mov  %ax, %ss

  /* 2) Switch to our C stack */
  mov  $__stack_top, %esp

  /* 3) Clear BSS section */
  call clear_bss

  /* 4) Enable paging (turn on MMU, identity map first 4 MB) */
  call setup_paging

  /* 5) Initialize PIC (remap IRQs 0–15 to 0x20–0x2F) */
  call init_pic

  /* 6) Build and load IDT */
  call init_idt

  sti                             /* enable interrupts */

  /* 7) Jump into C kernel */
  call kernel_main

.spin_halt:
  hlt                             /* if kernel_main returns, halt */
  jmp  .spin_halt

/*==============================================================================
  Data and tables
==============================================================================*/

/* ——— GDT ————————————————————————————————————————————————————————————————*/
.section .gdt
  .align 8
gdtr:
  .word gdt_end - gdt - 1          /* limit (size of GDT-1) */
  .long gdt                        /* base address */

gdt:
  /* NULL descriptor */
  .quad 0x0000000000000000

  /* CODE segment: base=0, limit=4 GiB, RING0, executable, read */
  .quad 0x00AF9A000000FFFF

  /* DATA segment: base=0, limit=4 GiB, RING0, writable, read */
  .quad 0x00AF92000000FFFF

gdt_end:

/* ——— IDT (empty, to be populated by init_idt) —————————————————————————*/
.section .idt
  .align 8
idtr:
  .word idt_end - idt - 1
  .long idt

idt:
  .fill 256*8, 1, 0              /* 256 empty 8-byte descriptors */
idt_end:

/* ——— Stack (16 KiB) and BSS clearing ————————————————————————————*/
.section .bss
  .align 16
  .skip 16*1024                  /* reserve 16 KiB stack */
__stack_bottom:
__stack_top:

/* Symbols for clear_bss to know what to zero */
  .globl __bss_start
  .globl __bss_end
__bss_start:
  . = ALIGN(16)
  /* actual uninitialized data lives after here */
  .skip 0                        /* markers only */
__bss_end:

/*==============================================================================
  Stubs for external C routines (must be implemented in C or assembly)
==============================================================================*/
  .extern clear_bss
  .extern setup_paging
  .extern init_pic
  .extern init_idt
  .extern kernel_main
```