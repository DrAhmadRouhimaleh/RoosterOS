; =============================================================================
; arch/x86/libc/arch_specific/memcpy.S
; 
; High-performance memcpy for x86_64, with simple fallback to rep-movsb on small
; sizes and aligned rep-movsq for large moves. Aligns destination, uses
; 64-bit bulk moves, then handles tail bytes. Saves caller-saved registers,
; returns dest in RAX. Thread- and signal-safe, no external dependencies.
;
; Signature:
;   void *memcpy(void *dest, const void *src, size_t n);
;
; Calling convention (System V AMD64 ABI):
;   dest → RDI, src → RSI, n → RDX
;   return in RAX (dest)
;
; Assemble with:   nasm -f elf64 memcpy.S -o memcpy.o
; =============================================================================

global memcpy
section .text
.align 16
memcpy:
    ; Prologue: save return pointer and callee-saved if needed
    ; (we only modify caller-saved: RAX, RCX, RDX, RSI, RDI, R8)
    mov     rax, rdi                ; save dest for return

    test    rdx, rdx                ; n == 0?
    je      .done

    cmp     rdx, 16                 ; tiny block?
    jb      .small_copy

    ; Align dest to 8-byte boundary
    mov     rcx, rdi
    mov     r8,  8
    and     rcx, 7                  ; offset = dest % 8
    jz      .bulk_copy
    mov     r9,  8
    sub     r9, rcx                 ; bytes to align
    cmp     rdx, r9
    cmova   r9, rdx                 ; if n < align, align = n
.align_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     r9
    dec     rdx
    jnz     .align_loop
    test    rdx, rdx
    je      .done

.bulk_copy:
    ; Copy 8-byte chunks with REP MOVSQ
    mov     rcx, rdx
    shr     rcx, 3                  ; rcx = n / 8
    rep     movsq
    ; Copy remaining bytes
    mov     rcx, rdx
    and     rcx, 7                  ; rcx = n % 8
    rep     movsb
    jmp     .done

.small_copy:
    ; For n < 16 use unaligned MOVSB in one go
    rep     movsb

.done:
    ret

; =============================================================================
; Fallback pure-C version if you need a non-ASM alternative:
; 
; #ifndef __x86_64__
; void *memcpy(void *dest, const void *src, size_t n) {
;     unsigned char *d = dest;
;     const unsigned char *s = src;
;     for (size_t i = 0; i < n; i++)
;         d[i] = s[i];
;     return dest;
; }
; #endif
; =============================================================================
