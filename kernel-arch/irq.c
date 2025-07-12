/* arch/x86/kernel-arch/pic.h
 * PIC driver API for x86 (8259A)
 */

#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* Initialization Control Words */
#define PIC_ICW1_INIT       0x10
#define PIC_ICW1_ICW4       0x01
#define PIC_ICW4_8086       0x01

/* End-Of-Interrupt command */
#define PIC_EOI             0x20

/* Default IRQ vector offsets */
#define PIC1_OFFSET         0x20
#define PIC2_OFFSET         0x28

/* I/O ports */
#define PIC1_CMD_PORT       0x20
#define PIC1_DATA_PORT      0x21
#define PIC2_CMD_PORT       0xA0
#define PIC2_DATA_PORT      0xA1

#ifdef __cplusplus
extern "C" {
#endif

/* Remap master/slave PIC to specified offsets */
void pic_remap(uint8_t offset1, uint8_t offset2);

/* Send EOI for given IRQ (0–15) */
void pic_send_eoi(uint8_t irq);

/* Mask/unmask individual IRQ line */
void pic_set_irq_mask(uint8_t irq_line);
void pic_clear_irq_mask(uint8_t irq_line);

/* Get/set full mask registers (lower 8 = PIC1, upper 8 = PIC2) */
uint16_t pic_get_mask(void);
void pic_set_mask(uint16_t mask);

/* Read IRR (Interrupt Request Register) and ISR (In-Service Register) */
uint16_t pic_read_irr(void);
uint16_t pic_read_isr(void);

#ifdef __cplusplus
}
#endif

#endif /* PIC_H */
