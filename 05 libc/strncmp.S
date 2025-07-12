; =============================================================================
; arch/x86/libc/arch_specific/strncmp.S
;
; int strncmp(const char *s1, const char *s2, size_t n);
;
; cdecl calling convention (32-bit x86):
;   [esp+4]  → s1
;   [esp+8]  → s2
;   [esp+12] → n
; Returns:
;   EAX = 0 if first n bytes are equal (or n==0 or hit NUL)
;       = (unsigned char)s1[i] – (unsigned char)s2[i] at first difference
;
; Features:
;  – Unrolls comparisons 4 bytes at a time for speed
;  – Early exit on NUL or difference
;  – Handles remaining bytes (<4) in a small loop
;  – No external dependencies, only uses EAX, EBX, ECX, ESI, EDI
;  – Preserves ESI, EDI, EBP
; =============================================================================

    section .text
    global  strncmp

strncmp:
    push    ebp
    mov     ebp, esp
    push    esi
    push    edi

    mov     edi, [ebp+8]      ; s1 → EDI
    mov     esi, [ebp+12]     ; s2 → ESI
    mov     ecx, [ebp+16]     ; n  → ECX
    test    ecx, ecx
    jz      .equal            ; if n==0 return 0

.loop_unroll:
    cmp     ecx, 4
    jl      .tail             ; fewer than 4 bytes? go to tail

    ; --- compare byte 0 ---
    mov     al, [edi]
    mov     bl, [esi]
    cmp     al, bl
    jne     .diff0
    test    al, al
    je      .equal

    ; --- compare byte 1 ---
    mov     al, [edi+1]
    mov     bl, [esi+1]
    cmp     al, bl
    jne     .diff1
    test    al, al
    je      .equal

    ; --- compare byte 2 ---
    mov     al, [edi+2]
    mov     bl, [esi+2]
    cmp     al, bl
    jne     .diff2
    test    al, al
    je      .equal

    ; --- compare byte 3 ---
    mov     al, [edi+3]
    mov     bl, [esi+3]
    cmp     al, bl
    jne     .diff3
    test    al, al
    je      .equal

    ; all 4 matched and not NUL → advance
    add     edi, 4
    add     esi, 4
    sub     ecx, 4
    jmp     .loop_unroll

; =============================================================================
; Difference handlers: compute (unsigned char)s1[i] – (unsigned char)s2[i]
; =============================================================================
.diff0:
    movzx   eax, byte [edi]
    movzx   ebx, byte [esi]
    sub     eax, ebx
    jmp     .cleanup

.diff1:
    movzx   eax, byte [edi+1]
    movzx   ebx, byte [esi+1]
    sub     eax, ebx
    jmp     .cleanup

.diff2:
    movzx   eax, byte [edi+2]
    movzx   ebx, byte [esi+2]
    sub     eax, ebx
    jmp     .cleanup

.diff3:
    movzx   eax, byte [edi+3]
    movzx   ebx, byte [esi+3]
    sub     eax, ebx
    jmp     .cleanup

; =============================================================================
; Tail loop for remaining bytes (ECX < 4)
; =============================================================================
.tail:
    test    ecx, ecx
    jz      .equal            ; no bytes left → equal

.tail_loop:
    movzx   eax, byte [edi]
    movzx   ebx, byte [esi]
    cmp     al, bl
    jne     .tail_diff
    test    al, al
    je      .equal            ; hit NUL → equal
    inc     edi
    inc     esi
    dec     ecx
    jnz     .tail_loop
    jmp     .equal

.tail_diff:
    sub     eax, ebx
    jmp     .cleanup

; =============================================================================
; Equal case
; =============================================================================
.equal:
    xor     eax, eax          ; return 0

; =============================================================================
; Epilogue
; =============================================================================
.cleanup:
    pop     edi
    pop     esi
    pop     ebp
    ret
