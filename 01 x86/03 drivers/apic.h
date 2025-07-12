/* arch/x86/kernel-arch/apic.h
 * Advanced Local APIC driver for x86 (Intel 82489DX+)
 *
 * Features:
 *   - MSR-based APIC base discovery
 *   - SVR setup (enable, spurious vector)
 *   - LVT setup: Timer, LINT0/1, Error, Thermal, Perf
 *   - One-shot and periodic timer support + calibration stub
 *   - EOI, IPI (Init, Startup, Fixed, NMI) with destination shorthand
 *   - Mask/unmask IRQs on LAPIC
 *   - Read ISR/IRR for debugging/spurious IRQ handling
 */

#ifndef ARCH_X86_KERNEL_APIC_H
#define ARCH_X86_KERNEL_APIC_H

#include <stdint.h>

/* IA32_APIC_BASE MSR */
#define MSR_IA32_APIC_BASE     0x1B
#define APIC_BASE_MSR_BSP      (1ULL << 8)
#define APIC_BASE_MSR_ENABLE   (1ULL << 11)
#define APIC_BASE_ADDR_MASK    0xFFFFF000ULL

/* Default physical relocation if identity-mapped */
#define APIC_DEFAULT_PHYS_ADDR 0xFEE00000UL

/* APIC register offsets */
enum {
  APIC_REG_ID          = 0x020,
  APIC_REG_VERSION     = 0x030,
  APIC_REG_TPR         = 0x080,
  APIC_REG_PPR         = 0x0A0,
  APIC_REG_EOI         = 0x0B0,
  APIC_REG_SVR         = 0x0F0,
  APIC_REG_ESR         = 0x280,
  APIC_REG_ICR_LO      = 0x300,
  APIC_REG_ICR_HI      = 0x310,
  APIC_REG_LVT_TIMER   = 0x320,
  APIC_REG_LVT_THERMAL = 0x330,
  APIC_REG_LVT_PERF    = 0x340,
  APIC_REG_LVT_LINT0   = 0x350,
  APIC_REG_LVT_LINT1   = 0x360,
  APIC_REG_LVT_ERROR   = 0x370,
  APIC_REG_TIMER_INIT  = 0x380,
  APIC_REG_TIMER_CUR   = 0x390,
  APIC_REG_TIMER_DIV   = 0x3E0,
};

/* LVT flags */
#define APIC_LVT_MASK         (1U << 16)
#define APIC_LVT_LEVEL_TRIG   (1U << 15)
#define APIC_LVT_ACTIVE_LOW   (1U << 13)

/* ICR delivery modes */
#define APIC_DELIVERY_FIXED   0x0
#define APIC_DELIVERY_LOWEST  0x1
#define APIC_DELIVERY_SMI     0x2
#define APIC_DELIVERY_NMI     0x4
#define APIC_DELIVERY_INIT    0x5
#define APIC_DELIVERY_STARTUP 0x6

/* ICR destination shorthand */
#define APIC_DEST_SELF        (1U << 18)
#define APIC_DEST_ALL_INC     (2U << 18)
#define APIC_DEST_ALL         (3U << 18)

/* Spurious vector: high bit enables APIC, low 8 bits = vector */
#define APIC_SVR_ENABLE       (1U << 8)
#define APIC_SVR_VECTOR(v)    ((v) & 0xFF)

/* Interface */
void    apic_init(void);
uint32_t apic_get_id(void);
uint32_t apic_get_version(void);
void    apic_send_eoi(void);
void    apic_send_ipi(uint8_t vector, uint32_t flags);
void    apic_mask_lvt(uint32_t reg_offset);
void    apic_unmask_lvt(uint32_t reg_offset);
void    apic_set_timer(uint8_t vector, uint8_t mode, uint32_t initial_count);
/* Calibration stub: measure bus clock vs. APIC timer */
uint32_t apic_calibrate_timer(uint32_t delay_ms);

#endif /* ARCH_X86_KERNEL_APIC_H */
