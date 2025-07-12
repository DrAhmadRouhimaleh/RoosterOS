/* arch/x86/kernel-arch/pic.c
 * PIC remapping and mask/EOI/IRR/ISR support
 */

#include "pic.h"
#include "io.h"    /* Provides outb(), inb(), io_wait() */

static inline void io_wait(void) {
    /* 0x80 is traditionally unused; write here for small delay */
    outb(0x80, 0);
}

/* Send command or data to PIC1/PIC2 */
static inline void pic_write(uint16_t port, uint8_t val) {
    outb(port, val);
    io_wait();
}

static inline uint8_t pic_read(uint16_t port) {
    uint8_t val = inb(port);
    io_wait();
    return val;
}

void pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t mask1 = pic_read(PIC1_DATA_PORT);
    uint8_t mask2 = pic_read(PIC2_DATA_PORT);

    /* Start initialization sequence in cascade mode */
    pic_write(PIC1_CMD_PORT, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    pic_write(PIC2_CMD_PORT, PIC_ICW1_INIT | PIC_ICW1_ICW4);

    /* Set vector offsets */
    pic_write(PIC1_DATA_PORT, offset1);
    pic_write(PIC2_DATA_PORT, offset2);

    /* Tell Master PIC there is a slave at IRQ2 (0000_0100) */
    pic_write(PIC1_DATA_PORT, 0x04);
    /* Tell Slave PIC its cascade identity (0000_0010) */
    pic_write(PIC2_DATA_PORT, 0x02);

    /* Set PICs to 8086 mode */
    pic_write(PIC1_DATA_PORT, PIC_ICW4_8086);
    pic_write(PIC2_DATA_PORT, PIC_ICW4_8086);

    /* Restore saved masks */
    pic_write(PIC1_DATA_PORT, mask1);
    pic_write(PIC2_DATA_PORT, mask2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        pic_write(PIC2_CMD_PORT, PIC_EOI);
    }
    pic_write(PIC1_CMD_PORT, PIC_EOI);
}

void pic_set_irq_mask(uint8_t irq_line) {
    uint16_t mask = pic_get_mask() | (1u << irq_line);
    pic_set_mask(mask);
}

void pic_clear_irq_mask(uint8_t irq_line) {
    uint16_t mask = pic_get_mask() & ~(1u << irq_line);
    pic_set_mask(mask);
}

uint16_t pic_get_mask(void) {
    uint8_t m1 = pic_read(PIC1_DATA_PORT);
    uint8_t m2 = pic_read(PIC2_DATA_PORT);
    return (uint16_t)m1 | ((uint16_t)m2 << 8);
}

void pic_set_mask(uint16_t mask) {
    pic_write(PIC1_DATA_PORT, (uint8_t)(mask & 0xFF));
    pic_write(PIC2_DATA_PORT, (uint8_t)((mask >> 8) & 0xFF));
}

uint16_t pic_read_irr(void) {
    /* OCW3: Read IRR (bit 0 = 0, bit 1 = 1) */
    pic_write(PIC1_CMD_PORT, 0x0A);
    pic_write(PIC2_CMD_PORT, 0x0A);
    uint8_t irr1 = pic_read(PIC1_CMD_PORT);
    uint8_t irr2 = pic_read(PIC2_CMD_PORT);
    return (uint16_t)irr1 | ((uint16_t)irr2 << 8);
}

uint16_t pic_read_isr(void) {
    /* OCW3: Read ISR (bit 0 = 1, bit 1 = 1) */
    pic_write(PIC1_CMD_PORT, 0x0B);
    pic_write(PIC2_CMD_PORT, 0x0B);
    uint8_t isr1 = pic_read(PIC1_CMD_PORT);
    uint8_t isr2 = pic_read(PIC2_CMD_PORT);
    return (uint16_t)isr1 | ((uint16_t)isr2 << 8);
}
