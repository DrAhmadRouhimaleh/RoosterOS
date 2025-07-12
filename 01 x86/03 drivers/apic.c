/* arch/x86/kernel-arch/apic.c
 * Advanced Local APIC driver implementation
 */

#include "apic.h"
#include "msr.h"    /* read_msr, write_msr */
#include "io.h"     /* io_wait() if needed */
#include <stddef.h>

static volatile uint8_t *lapic = NULL;

/* Internal helpers */
static inline uint32_t lapic_read(uint32_t reg)
{
    return *((volatile uint32_t *)(lapic + reg));
}

static inline void lapic_write(uint32_t reg, uint32_t val)
{
    *((volatile uint32_t *)(lapic + reg)) = val;
    /* ensure write ordering */
    (void)lapic_read(APIC_REG_ID);
}

static void map_lapic(void)
{
    uint64_t msr = read_msr(MSR_IA32_APIC_BASE);
    uint32_t phys = (uint32_t)(msr & APIC_BASE_ADDR_MASK);
    /* Must map 'phys' into kernel virtual space; assume identity-mapped */
    lapic = (volatile uint8_t *)(phys ?: APIC_DEFAULT_PHYS_ADDR);
}

/* Public API */

void apic_init(void)
{
    map_lapic();

    /* Enable APIC in MSR */
    uint64_t msr = read_msr(MSR_IA32_APIC_BASE);
    msr |= APIC_BASE_MSR_ENABLE;
    write_msr(MSR_IA32_APIC_BASE, msr);

    /* Spurious Interrupt Vector Register: enable + vector 0xFF */
    lapic_write(APIC_REG_SVR,
                APIC_SVR_ENABLE | APIC_SVR_VECTOR(0xFF));

    /* Mask out all LVT entries except error (we’ll unmask as needed) */
    lapic_write(APIC_REG_LVT_TIMER,   APIC_LVT_MASK);
    lapic_write(APIC_REG_LVT_THERMAL, APIC_LVT_MASK);
    lapic_write(APIC_REG_LVT_PERF,    APIC_LVT_MASK);
    lapic_write(APIC_REG_LVT_LINT0,   APIC_LVT_MASK | APIC_LVT_LEVEL_TRIG);
    lapic_write(APIC_REG_LVT_LINT1,   APIC_LVT_MASK | APIC_LVT_LEVEL_TRIG);
    /* allow error interrupts */
    lapic_write(APIC_REG_LVT_ERROR,   APIC_LVT_LEVEL_TRIG);

    /* Clear error status by reading/writing ESR */
    lapic_write(APIC_REG_ESR, 0);
    lapic_write(APIC_REG_ESR, 0);

    /* Send INIT IPI to self to synchronise arbitration ID */
    apic_send_ipi(0, APIC_DELIVERY_INIT | APIC_DEST_SELF);

    /* Calibrate timer if needed (stub) */
    /* uint32_t ticks = apic_calibrate_timer(10); */

    /* Example: one-shot 1000Hz timer on vector 0xF0 */
    apic_set_timer(0xF0, /* mode = 1 for periodic, 0 for one-shot */ 1, 1000000);

    /* Clear any pending EOI */
    apic_send_eoi();
}

uint32_t apic_get_id(void)
{
    return lapic_read(APIC_REG_ID) >> 24;
}

uint32_t apic_get_version(void)
{
    return lapic_read(APIC_REG_VERSION) & 0xFF;
}

void apic_send_eoi(void)
{
    lapic_write(APIC_REG_EOI, 0);
}

void apic_send_ipi(uint8_t vector, uint32_t flags)
{
    /* Write high dword first if needed (no dest in shorthands) */
    lapic_write(APIC_REG_ICR_HI, 0);
    lapic_write(APIC_REG_ICR_LO,
                (vector & 0xFF)
                | (flags & 0x0007'0000)    /* destination shorthand */
                | ((flags & 0x0000'0007) << 8)); /* delivery mode */
    /* Wait for delivery */
    while (lapic_read(APIC_REG_ICR_LO) & (1U << 12)) { }
}

void apic_mask_lvt(uint32_t reg_offset)
{
    uint32_t v = lapic_read(reg_offset);
    lapic_write(reg_offset, v | APIC_LVT_MASK);
}

void apic_unmask_lvt(uint32_t reg_offset)
{
    uint32_t v = lapic_read(reg_offset);
    lapic_write(reg_offset, v & ~APIC_LVT_MASK);
}

void apic_set_timer(uint8_t vector, uint8_t periodic, uint32_t initial_count)
{
    /* Divide configuration: divide by 16 */
    lapic_write(APIC_REG_TIMER_DIV, 0x3);
    /* Set LVT timer entry */
    lapic_write(APIC_REG_LVT_TIMER,
                (vector & 0xFF)
                | (periodic ? (1U << 17) : 0));
    /* Start the timer */
    lapic_write(APIC_REG_TIMER_INIT, initial_count);
}

/* Stub: measure <delay_ms> using PIT or HPET to calibrate APIC timer freq */
uint32_t apic_calibrate_timer(uint32_t delay_ms)
{
    /* TODO: implement PIT-based measurement: program APIC one-shot with
     * max count, wait delay_ms using PIT, then read APIC current count.
     * Return (initial_count - current_count) / delay_ms to get ticks/ms.
     */
    return 0;
}
