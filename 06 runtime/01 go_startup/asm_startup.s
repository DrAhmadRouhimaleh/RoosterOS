; =============================================================================
; rooster-os/arch/x86/runtime/go_startup/asm_startup.s
;
; 64-bit x86_64 Go runtime entry stub for RoosterOS
; • Disables interrupts
; • Sets up a dedicated stack
; • Clears BSS (zero-init globals)
; • Initializes thread-local storage (TLS)
; • Passes Multiboot parameters to runtime_start(magic, mbi)
; • Halts if runtime_start ever returns
;
; Assembled with:
;   gcc –c –nostdlib –o asm_startup.o asm_startup.s
; Linked via your custom linker script defining _start, runtime_start,
; __bss_start, __bss_end, and any Go data sections.
; =============================================================================

    .text
    .globl _start
    .type  _start,@function
_start:
    cli                             # disable external interrupts

    # --- 1) Set up the hardware stack ---
    lea     stack_top(%rip), %rsp   # point RSP at top of stack_space region

    # --- 2) Zero BSS (globals/data) ---
    lea     __bss_start(%rip), %rdi  # RDI = start of BSS
    lea     __bss_end(%rip), %rcx    # RCX = end of BSS
    sub     %rdi, %rcx               # RCX = size of BSS
    xor     %rax, %rax               # AL = 0
    rep     stosb                    # memset(__bss_start, 0, __bss_end-__bss_start)

    # --- 3) Initialize TLS for Go runtime ---
    lea     tls_block(%rip), %rax    # address of TLS area
    mov     %rax, %fs:0              # set FS.base = tls_block (requires kernel support)
    # For bare-metal you may need to configure MSRs via WRMSR here for FS_BASE.

    # --- 4) Branch prediction hint and pipeline flush ---
    lfence                           # serialize loads
    mfence                           # ensure previous stores complete

    # --- 5) Prepare arguments for runtime_start(uint32, uintptr) ---
    #   Multiboot loader placed:
    #     magic   → EAX
    #     mbi_ptr → EBX
    mov     %eax, %edi               # edi = magic (zero-extended)
    mov     %ebx, %rsi               # rsi = mbi_ptr

    # --- 6) Call Go runtime entry ---
    call    runtime_start

.hang:
    hlt                              # halt CPU (low power wait for interrupt)
    jmp     .hang

    .size _start, .-_start

; =============================================================================
; BSS Layout and Stack Allocation
; =============================================================================
    .section .bss
    .align 16
stack_space:
    .zero 0x20000                    # reserve 128 KiB for stack (grows down)
stack_top:

    .align 16
tls_block:
    .zero 0x1000                     # reserve 4 KiB for Go TLS data (G, M, etc.)

    .align 16
__bss_start:
    ; (Place global, zero-init Go variables or C globals here)
    .zero 0                          # marker
__bss_end:

    .ident "RoosterOS Go startup stub v1.0"
