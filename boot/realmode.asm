; =============================================================================
;  arch/x86/boot/realmode.asm
;  16-bit real-mode bootloader stub for RoosterOS
;
;  • Sets up a minimal stack in real mode
;  • Enables A20 (fast method + keyboard controller fallback)
;  • Loads a flat GDT for 32-bit protected mode
;  • Jumps into protected mode, reloads segment registers
;  • Calls C entry point `enter_protected_mode`
;  • Stalls/halt on return
;
;  Assemble with NASM:
;    nasm -f bin realmode.asm -o boot.bin
; =============================================================================

BITS 16
ORG 0x7C00

;------------------------------------------------------------------------------
; Constants
;------------------------------------------------------------------------------
CODE_SEL    equ 0x08       ; GDT selector: 2nd entry, descriptor index 1
DATA_SEL    equ 0x10       ; GDT selector: 3rd entry, descriptor index 2
STACK_REAL  equ 0x0000     ; real-mode stack segment
STACK_OFF   equ 0x7C00     ; real-mode stack offset

;------------------------------------------------------------------------------
; Entry Point: Real Mode Initialization
;------------------------------------------------------------------------------
start:
    cli                        ; disable interrupts
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, STACK_REAL
    mov     sp, STACK_OFF      ; stack at 0x0000:0x7C00

    ; Enable A20 line
    call    enable_a20

    ; Load GDT
    lgdt    [gdt_descriptor]

    ; Enter Protected Mode
    mov     eax, cr0
    or      eax, 1            ; set PE bit (Protection Enable)
    mov     cr0, eax

    ; Far jump to flush prefetch and load CS=CODE_SEL
    jmp     CODE_SEL:protected_entry

;------------------------------------------------------------------------------
; A20 Enable Routine (fast + fallback)
;------------------------------------------------------------------------------
enable_a20:
    push    dx
    cli

    ; --- Fast enable via port 0x92 ---
    in      al, 0x92
    or      al, 00000010b      ; set A20 on
    out     0x92, al

    ; --- Verify or fallback to keyboard controller ---
    in      al, 0x64
    test    al, 00000010b      ; IBF set? if set, fast method worked
    jnz     .done_fast

    ; Wait for input buffer empty
.wait_ibf_clear:
    in      al, 0x64
    test    al, 00000002h
    jnz     .wait_ibf_clear

    mov     al, 0xD1           ; command: write output port
    out     0x64, al
.wait_data:
    in      al, 0x64
    test    al, 00000002h      ; wait input buffer empty
    jnz     .wait_data

    mov     al, 0xDF           ; bit1 = A20, others unchanged
    out     0x60, al

.done_fast:
    sti
    pop     dx
    ret

;------------------------------------------------------------------------------
; GDT Definition: flat 4GB code/data segments
;------------------------------------------------------------------------------
gdt_start:
    dq      0x0000000000000000      ; [0] null descriptor
    dq      0x00CF9A000000FFFF      ; [1] code: base=0, limit=4GB, DPL=0, 32-bit
    dq      0x00CF92000000FFFF      ; [2] data: base=0, limit=4GB, DPL=0, 32-bit
gdt_end:

gdt_descriptor:
    dw      gdt_end - gdt_start - 1  ; limit (size-1)
    dd      gdt_start                ; base address

; Switch assembler to 32-bit mode for protected_entry
BITS 32

;------------------------------------------------------------------------------
; Protected Mode Entry
;------------------------------------------------------------------------------
protected_entry:
    ; Reinitialize segment registers to DATA_SEL
    mov     ax, DATA_SEL
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; Set up stack in protected-mode (e.g. 0x90000)
    mov     esp, 0x00090000

    ; Call C initialization routine
    extern  enter_protected_mode
    call    enter_protected_mode

    ; If it returns, halt the CPU
.halt:
    hlt
    jmp     .halt

;------------------------------------------------------------------------------
; Boot Signature (must be at offset 510–511)
;------------------------------------------------------------------------------
BITS 16
times 510 - ($ - $$) db 0
dw    0xAA55
