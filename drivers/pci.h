// arch/x86/drivers/pci.h
#ifndef ARCH_X86_DRIVERS_PCI_H
#define ARCH_X86_DRIVERS_PCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Maximum devices we’ll track */
#define PCI_MAX_DEVICES 256

/* PCI header types */
#define PCI_HEADER_TYPE_DEVICE  0x00
#define PCI_HEADER_TYPE_BRIDGE  0x01
#define PCI_HEADER_MULTIFUNC    0x80

/* PCI BARs */
#define PCI_NUM_BARS_DEVICE     6
#define PCI_NUM_BARS_BRIDGE     2

/* Representation of one PCI BAR */
typedef struct {
    bool     valid;            /* BAR implemented? */
    bool     mmio;             /* 0 = I/O port, 1 = Memory */
    bool     prefetchable;     /* memory prefetchable */
    uint64_t addr;             /* base address */
    uint64_t size;             /* size of the window */
} pci_bar_t;

/* PCI device record */
typedef struct {
    uint8_t     bus;
    uint8_t     device;
    uint8_t     function;

    uint16_t    vendor_id;
    uint16_t    device_id;

    uint8_t     class_code;
    uint8_t     subclass;
    uint8_t     prog_if;
    uint8_t     header_type;
    uint8_t     revision_id;

    uint8_t     irq_line;
    uint8_t     irq_pin;

    size_t      bar_count;
    pci_bar_t   bars[PCI_NUM_BARS_DEVICE];
} pci_device_t;

/* Global array of discovered devices */
extern pci_device_t pci_devices[PCI_MAX_DEVICES];
extern size_t       pci_device_count;

/* Raw config-space accessors */
uint32_t pci_config_read32(uint8_t bus, uint8_t device,
                           uint8_t func, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t device,
                           uint8_t func, uint8_t offset);
uint8_t  pci_config_read8 (uint8_t bus, uint8_t device,
                           uint8_t func, uint8_t offset);

void pci_config_write32(uint8_t bus, uint8_t device,
                        uint8_t func, uint8_t offset,
                        uint32_t value);
void pci_config_write16(uint8_t bus, uint8_t device,
                        uint8_t func, uint8_t offset,
                        uint16_t value);
void pci_config_write8 (uint8_t bus, uint8_t device,
                        uint8_t func, uint8_t offset,
                        uint8_t value);

/* Initialize the PCI subsystem and build pci_devices[] */
void pci_init(void);

/* Find first device matching vendor and device IDs (or NULL) */
pci_device_t *pci_find_device(uint16_t vendor_id,
                              uint16_t device_id);

#endif // ARCH_X86_DRIVERS_PCI_H
