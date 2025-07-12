/*
 * arch/x86/libc/arch_specific/div64.c
 *
 * High-performance 64-bit by 32-bit division for IA-32 (x86).
 * Provides both unsigned and signed variants using the DIV/IDIV
 * instructions. Handles divide-by-zero by returning saturated
 * values and storing the full dividend as remainder.
 *
 * Functions:
 *   uint32_t udiv64_32(uint64_t dividend,
 *                     uint32_t divisor,
 *                     uint64_t *remainder);
 *
 *   int32_t  sdiv64_32(int64_t  dividend,
 *                     int32_t  divisor,
 *                     int64_t  *remainder);
 *
 * If you need a pure-C fallback (for non-IA-32 targets), you can
 * #ifdef out the inline asm and provide a C implementation.
 */

#include <stdint.h>
#include <limits.h>

/*
 * Unsigned 64-bit dividend divided by 32-bit divisor.
 *
 * @param dividend  the 64-bit unsigned dividend
 * @param divisor   the 32-bit unsigned divisor
 * @param remainder out-param: if non-NULL, receives the 64-bit remainder
 * @return 32-bit quotient (UINT32_MAX if divisor == 0)
 */
uint32_t udiv64_32(uint64_t dividend,
                   uint32_t divisor,
                   uint64_t *remainder)
{
    if (divisor == 0) {
        if (remainder)
            *remainder = dividend;
        return UINT32_MAX;
    }

    uint32_t quot, rem;

    /*
     * Load dividend into EDX:EAX and divide by divisor:
     *
     *   EDX:EAX ← dividend
     *   DIV  r/m32
     *   → EAX = quotient (32 bits)
     *   → EDX = remainder (32 bits)
     */
    __asm__ volatile (
        "divl %[d]\n"
        : "=a"(quot), "=d"(rem)
        : "a"((uint32_t)dividend),            /* low  32 bits → EAX */
          "d"((uint32_t)(dividend >> 32)),    /* high 32 bits → EDX */
          [d]"r"(divisor)                     /* divisor in register */
        : "cc"
    );

    if (remainder)
        *remainder = (uint64_t)rem;
    return quot;
}

/*
 * Signed 64-bit dividend divided by 32-bit divisor.
 *
 * @param dividend  the 64-bit signed dividend
 * @param divisor   the 32-bit signed divisor
 * @param remainder out-param: if non-NULL, receives the signed 64-bit remainder
 * @return 32-bit quotient (INT32_MAX/INT32_MIN if divisor == 0)
 */
int32_t sdiv64_32(int64_t dividend,
                  int32_t divisor,
                  int64_t *remainder)
{
    if (divisor == 0) {
        if (remainder)
            *remainder = dividend;
        /* Saturate on divide-by-zero */
        return (dividend < 0) ? INT32_MIN : INT32_MAX;
    }

    int32_t quot, rem;

    /*
     * Load dividend into EDX:EAX (sign-extended) then IDIV:
     *
     *   EAX = low  32 bits of dividend
     *   EDX = high 32 bits of dividend (sign-extended)
     *   IDIV r/m32
     *   → EAX = quotient
     *   → EDX = remainder
     */
    __asm__ volatile (
        "idivl %[d]\n"
        : "=a"(quot), "=d"(rem)
        : "a"((uint32_t)dividend),
          "d"((uint32_t)(dividend >> 32)),
          [d]"r"(divisor)
        : "cc"
    );

    if (remainder)
        *remainder = (int64_t)rem;
    return quot;
}

/* 
 * Pure-C fallback for non-IA-32 targets or compilers without
 * inline assembly support. Uncomment and adjust as needed.
 */
/*
#ifndef __i386__
uint32_t udiv64_32(uint64_t dividend,
                   uint32_t divisor,
                   uint64_t *remainder)
{
    if (divisor == 0) {
        if (remainder) *remainder = dividend;
        return UINT32_MAX;
    }
    uint64_t q = dividend / divisor;
    if (remainder) *remainder = dividend - q * divisor;
    return (uint32_t)q;
}

int32_t sdiv64_32(int64_t dividend,
                  int32_t divisor,
                  int64_t *remainder)
{
    if (divisor == 0) {
        if (remainder) *remainder = dividend;
        return (dividend < 0) ? INT32_MIN : INT32_MAX;
    }
    int64_t q = dividend / divisor;
    if (remainder) *remainder = dividend - q * divisor;
    return (int32_t)q;
}
#endif
*/
