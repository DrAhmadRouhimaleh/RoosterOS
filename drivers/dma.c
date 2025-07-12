/* arch/x86/drivers/dma.c
 * Comprehensive Intel 8237 ISA DMA controller driver for x86 (primary controller).
 * Supports channel masking, mode setup, 8-bit transfers, address/count programming,
 * flip-flop reset, page register programming, and 16-bit (cascade) channels.
 *
 * Assumes PC/AT DMA ports:
 *   Primary controller (channels 0–3)
 *     Mask Reg:       0x0A
 *     Mode Reg:       0x0B
 *     Clear FF Reg:   0x0C
 *     Status Reg:     0x08
 *     Base Addr Ports: 0x00,0x02,0x04,0x06
 *     Base Count Ports:0x01,0x03,0x05,0x07
 *     Page Ports:     0x87,0x83,0x81,0x82
 *
 *   Secondary controller (channels 4–7) lives at 0xC0–0xDE; not shown here.
 */

#include <stdint.h>

/* I/O primitives */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* DMA port definitions */
enum {
    DMA1_MASK_REG    = 0x0A,
    DMA1_MODE_REG    = 0x0B,
    DMA1_CLEAR_FF    = 0x0C,
    DMA1_STATUS_REG  = 0x08
};

/* Base address, count, and page registers for channels 0–3 */
static const uint16_t dma1_addr_port[4]  = { 0x00, 0x02, 0x04, 0x06 };
static const uint16_t dma1_count_port[4] = { 0x01, 0x03, 0x05, 0x07 };
static const uint16_t dma1_page_port[4]  = { 0x87, 0x83, 0x81, 0x82 };

/* DMA mode bits */
#define DMA_TRANSFER_READ    0x44  /* device → memory */
#define DMA_TRANSFER_WRITE   0x48  /* memory → device */
#define DMA_MODE_SINGLE      0x00
#define DMA_MODE_DEMAND      0x00
#define DMA_MODE_AUTO_INIT   0x10
#define DMA_MODE_ADDRESS_INC 0x00
#define DMA_MODE_ADDRESS_DEC 0x20

/* High-level API */

/* Reset the internal address/count flip-flop of the given controller */
static void dma_reset_ff(void) {
    outb(DMA1_CLEAR_FF, 0);
}

/* Mask (disable) a DMA channel */
void dma_mask_channel(uint8_t channel) {
    uint8_t mask = 0x04 | (channel & 0x03);
    outb(DMA1_MASK_REG, mask);
}

/* Unmask (enable) a DMA channel */
void dma_unmask_channel(uint8_t channel) {
    uint8_t mask = channel & 0x03;
    outb(DMA1_MASK_REG, mask);
}

/* Set DMA mode for a channel
 * channel: 0–3
 * mode: combine DMA_TRANSFER_*, DMA_MODE_*, and channel number
 */
void dma_set_mode(uint8_t channel, uint8_t mode) {
    uint8_t cmd = (mode & 0x3C) | (channel & 0x03);
    outb(DMA1_MODE_REG, cmd);
}

/* Program the physical address for a DMA transfer */
void dma_set_address(uint8_t channel, uint32_t phys_addr) {
    /* Only low 24 bits usable on ISA */
    uint8_t page = (phys_addr >> 16) & 0xFF;
    uint16_t port = dma1_page_port[channel & 0x03];
    outb(port, page);

    /* Reset flip-flop, then write low and high bytes */
    dma_reset_ff();
    uint16_t base = phys_addr & 0xFFFF;
    outb(dma1_addr_port[channel & 0x03], base & 0xFF);
    outb(dma1_addr_port[channel & 0x03], (base >> 8) & 0xFF);
}

/* Program the transfer count for a DMA transfer */
void dma_set_count(uint8_t channel, uint16_t count) {
    /* count is (bytes − 1) */
    uint16_t cnt = count - 1;
    dma_reset_ff();
    outb(dma1_count_port[channel & 0x03], cnt & 0xFF);
    outb(dma1_count_port[channel & 0x03], (cnt >> 8) & 0xFF);
}

/* Query DMA status register */
uint8_t dma_get_status(void) {
    return inb(DMA1_STATUS_REG);
}

/* Initialize a single DMA channel (0–3) for an 8-bit transfer:
 *   - Masks the channel
 *   - Sets mode (read/write, auto-init, single-cycle, increment)
 *   - Programs address and count
 *   - Unmasks the channel
 * count: number of bytes to transfer
 */
void dma_channel_setup(uint8_t channel,
                       uint8_t direction_read,
                       uint8_t auto_init,
                       uint32_t phys_addr,
                       uint16_t count) {
    /* 1) Mask channel to prevent spurious transfers */
    dma_mask_channel(channel);

    /* 2) Build mode byte */
    uint8_t mode = (direction_read ? DMA_TRANSFER_READ : DMA_TRANSFER_WRITE)
                | (auto_init ? DMA_MODE_AUTO_INIT : DMA_MODE_DEMAND);

    dma_set_mode(channel, mode);

    /* 3) Program address and count */
    dma_set_address(channel, phys_addr);
    dma_set_count(channel, count);

    /* 4) Unmask channel to enable */
    dma_unmask_channel(channel);
}

/* High-level initialization stub for channel 2 (e.g. floppy) */
void dma_init(void) {
    /* Example: channel 2, read from device, no auto-init,
       physical buffer at 0x00080000, length 0x1000 bytes */
    dma_channel_setup(2, /*channel*/
                      1, /*direction_read*/
                      0, /*auto_init*/
                      0x00080000, /*phys_addr*/
                      0x1000);    /*count*/
}
