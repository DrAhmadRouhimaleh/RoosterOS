/* arch/x86/boot/linker.ld
 * Advanced link script for a 16-bit bootloader (single 512-byte sector)
 */

OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386)
ENTRY(start)

SECTIONS
{
  /* Set load address to 0x7C00 (BIOS loads boot sector here) */
  . = 0x7C00;

  /* ========================
     Boot code (.text) region
     ======================== */
  .text : ALIGN(16)
  {
    /* Symbol the CPU jumps to */
    PROVIDE(start = .);

    /* Optional Multiboot header (if using GRUB or similar) */
    *(.multiboot_header)

    /* Boot-stage entry point code */
    *(.text.boot)

    /* All other code fragments */
    *(.text*)
  }

  /* Pad up to the next 512-byte boundary (one sector) */
  . = ALIGN(512);

  /* =========================
     Read-only data (.rodata)
     ========================= */
  .rodata : ALIGN(16)
  {
    *(.rodata*)
  }

  /* =========================
     Initialized data (.data)
     ========================= */
  .data : ALIGN(16)
  {
    *(.data*)
  }

  /* ===============================
     Uninitialized data (.bss, COMMON)
     =============================== */
  .bss : ALIGN(16)
  {
    __bss_start = .;   /* Mark start of BSS */
    *(.bss*)
    *(COMMON)
    __bss_end = .;     /* Mark end of BSS */
  }

  /* =====================================
     Discard sections not needed at runtime
     ===================================== */
  /DISCARD/ :
  {
    *(.comment)
    *(.note*)
    *(.eh_frame)
    *(.gnu*)
  }

  /* =================================================
     Pad to offset 510 bytes, then write the boot signature
     ================================================= */
  . = 0x7C00 + 510;
  BYTE(0x55)
  BYTE(0xAA)
}
