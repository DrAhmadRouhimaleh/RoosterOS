; =============================================================================
; arch/x86/libc/arch_specific/memset.S
;
; void *memset(void *dest, int c, size_t n);
;
; 32-bit x86 cdecl:
;   [esp+4]  = dest
;   [esp+8]  = c (int, low byte used)
;   [esp+12] = n (size_t)
;
; Features:
;  - Align dest to 4-byte boundary
;  - Bulk fill with REP STOSD
;  - Tail bytes with REP STOSB
;  - SSE2 path for large buffers (≥ 128 bytes), using MOVDQA/PSHUFD + MOVNTDQ
;  - Runtime CPUID check for SSE2; cached in .data
;  - Preserves FLAGS, returns dest in EAX
; =============================================================================

section .data
    have_sse2:      dd 0          ; 0=unknown, 1=yes, -1=no

section .text
    global memset
    extern memcpy         ; to use if fallback needed

align 16
memset:
    push    ebp
    mov     ebp, esp
    push    ebx
    push    esi
    push    edi
    pushfd

    cld                         ; forward direction
    mov     edi, [ebp+8]        ; dest
    mov     eax, [ebp+8]        ; prepare return in EAX
    mov     eax, edi

    mov     ecx, [ebp+16]       ; n
    test    ecx, ecx
    jz      .done

    mov     bl, [ebp+12]        ; c (byte)
    mov     bh, bl
    shl     ebx, 16
    or      ebx, ebx            ; EBX = 0x00BB00BB
    mov     ebx, ebx            ; expand to 32-bit
    mov     bh, bl              ; now EBX = 0xBBBBBBBB

    ; choose path: use SSE2 for large n
    cmp     ecx, 128
    jl      .dword_path
    call    .detect_sse2
    cmp     dword [have_sse2], 1
    jne     .dword_path
    jmp     .sse2_path

; -------------------------------------------------------------------
; 4-byte-aligned bulk path with REP STOSD, then bytes
; -------------------------------------------------------------------
.dword_path:
    ; align dest to 4-byte boundary
    mov     edx, edi
    and     edx, 3
    jz      .stosd_align_done
    mov     eax, edx
    mov     esi, 4
    sub     esi, edx            ; bytes to align = 4 - (dest%4)
    cmp     ecx, esi
    cmova   esi, ecx
.align4_loop:
    mov     al, bl
    stosb
    dec     ecx
    dec     esi
    jnz     .align4_loop
.stosd_align_done:
    ; bulk fill dwords
    mov     eax, ebx            ; pattern in EAX
    mov     edx, ecx
    shr     ecx, 2
    rep     stosl
    mov     ecx, edx
    and     ecx, 3
    rep     stosb
    jmp     .restore

; -------------------------------------------------------------------
; SSE2-accelerated path (for n≥128, SSE2 available)
; -------------------------------------------------------------------
.sse2_path:
    ; align dest to 16-byte boundary
    mov     edx, edi
    and     edx, 15
    jz      .sse2_bulk
    mov     esi, 16
    sub     esi, edx            ; bytes to align
    cmp     ecx, esi
    cmova   esi, ecx
.sse2_align_loop:
    mov     al, bl
    stosb
    dec     ecx
    dec     esi
    jnz     .sse2_align_loop

.sse2_bulk:
    ; prepare XMM0 with pattern
    movd    xmm0, ebx
    pshufd  xmm0, xmm0, 0
    ; bulk store 16-byte chunks with non‐temporal stores
    mov     edx, ecx
    shr     ecx, 4              ; count = n/16
    pxor    xmm1, xmm1         ; empty reg for prefetch
.sse2_loop:
    movntdq [edi], xmm0
    add     edi, 16
    dec     ecx
    jnz     .sse2_loop
    sfence                      ; order non‐temporal stores
    mov     ecx, edx
    and     ecx, 15
    rep     stosb
    jmp     .restore

; -------------------------------------------------------------------
; CPUID-based SSE2 detection (cached)
; -------------------------------------------------------------------
.detect_sse2:
    cmp     dword [have_sse2], 0
    jne     .cpuid_done
    ; call cpuid
    push    ebx
    mov     eax, 1
    cpuid
    test    edx, (1 << 26)      ; SSE2 bit
    jz      .no_sse2
    mov     dword [have_sse2], 1
    jmp     .cpuid_done
.no_sse2:
    mov     dword [have_sse2], -1
.cpuid_done:
    pop     ebx
    ret

; -------------------------------------------------------------------
; Restore registers & return
; -------------------------------------------------------------------
.restore:
    popfd
    pop     edi
    pop     esi
    pop     ebx
    mov     esp, ebp
    pop     ebp
.done:
    ret
