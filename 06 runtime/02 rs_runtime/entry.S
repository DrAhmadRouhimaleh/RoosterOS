// arch/x86/runtime/rs_runtime/entry.S
// 64-bit x86_64 Rust runtime entry stub for RoosterOS
// • Preserves Multiboot handoff registers
// • Clears BSS
// • Sets up stack and optional TLS
// • Passes magic & mbi ptr to rust_start()
// • Halts if rust_start ever returns

    .intel_syntax noprefix
    .text
    .globl _start
    .type  _start,@function

_start:
    // 1) Save Multiboot registers (magic in EAX, mbi ptr in EBX)
    mov     r12d, eax            // r12d ← magic
    mov     r13,  rbx            // r13  ← mbi_addr

    // 2) Disable interrupts
    cli

    // 3) Clear BSS segment [__bss_start, __bss_end)
    lea     rdi, [rip + __bss_start]
    lea     rcx, [rip + __bss_end]
    sub     rcx, rdi             // rcx = size of BSS
    xor     rax, rax             // al = 0
    rep     stosb                // memset(__bss_start, 0, __bss_end-__bss_start)

    // 4) Initialize stack pointer at top of stack_space
    lea     rsp, [rip + stack_top]

    // 5) (Optional) Set up FS-base for thread-local storage (TLS)
    //    IA32_FS_BASE MSR = 0xC0000100
    mov     ecx, 0xC0000100
    lea     rax, [rip + tls_start]
    writemsr                     // write rdx:rax → MSR[ecx]

    // 6) Restore handoff registers into argument registers
    mov     edi, r12d            // edi ← magic (uint32)
    mov     esi, r13d            // esi ← mbi_addr (uintptr)

    // 7) Call Rust entry point (in rt.rs)
    extern  rust_start
    call    rust_start

// 8) If rust_start returns, halt forever
.hang:
    hlt
    jmp     .hang

    .size _start, .-_start

//------------------------------------------------------------------------------
// BSS, Stack & TLS Layout
//------------------------------------------------------------------------------ 
    .section .bss
    .align 16

// 128 KiB stack (grows down)
stack_space:
    .zero   0x20000
stack_top:

// 4 KiB TLS block for Rust (GDT/IDT-dependent)
.align 16
tls_start:
    .zero   0x1000
tls_end:

// Marker symbols for BSS clearing
.align 16
__bss_start:
    .zero   0           // start of zero-init data
__bss_end:

    .ident "RoosterOS x86_64 Rust startup v1.0"
