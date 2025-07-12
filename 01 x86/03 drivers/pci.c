// arch/x86/drivers/pci.c
#include "pci.h"
#include "io.h"        // outl, inl, outb, inb
#include <string.h>    // memset, memcpy

pci_device_t pci_devices[PCI_MAX_DEVICES];
size_t       pci_device_count = 0;

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

/* Build the 32-bit CONFIG_ADDRESS value */
static inline uint32_t pci_make_addr(uint8_t bus, uint8_t dev,
                                     uint8_t func, uint8_t offset) {
    return (uint32_t)((1U << 31)           /* enable bit */
        | (bus  << 16)
        | (dev  << 11)
        | (func <<  8)
        | (offset & 0xFC));
}

/* Raw config-space readers/writers */
uint32_t pci_config_read32(uint8_t bus, uint8_t dev,
                           uint8_t func, uint8_t offset) {
    uint32_t addr = pci_make_addr(bus, dev, func, offset);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t dev,
                           uint8_t func, uint8_t offset) {
    uint32_t v = pci_config_read32(bus, dev, func, offset & ~2);
    return (uint16_t)(v >> ((offset & 2) * 8));
}

uint8_t pci_config_read8(uint8_t bus, uint8_t dev,
                         uint8_t func, uint8_t offset) {
    uint32_t v = pci_config_read32(bus, dev, func, offset & ~3);
    return (uint8_t)(v >> ((offset & 3) * 8));
}

void pci_config_write32(uint8_t bus, uint8_t dev,
                        uint8_t func, uint8_t offset,
                        uint32_t value) {
    uint32_t addr = pci_make_addr(bus, dev, func, offset);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, value);
}

void pci_config_write16(uint8_t bus, uint8_t dev,
                        uint8_t func, uint8_t offset,
                        uint16_t value) {
    uint8_t shift = (offset & 2) * 8;
    uint32_t v = pci_config_read32(bus, dev, func, offset & ~2);
    v = (v & ~(0xFFFFu << shift)) | ((uint32_t)value << shift);
    pci_config_write32(bus, dev, func, offset & ~2, v);
}

void pci_config_write8(uint8_t bus, uint8_t dev,
                       uint8_t func, uint8_t offset,
                       uint8_t value) {
    uint8_t shift = (offset & 3) * 8;
    uint32_t v = pci_config_read32(bus, dev, func, offset & ~3);
    v = (v & ~(0xFFu << shift)) | ((uint32_t)value << shift);
    pci_config_write32(bus, dev, func, offset & ~3, v);
}

/* Probe one function, record it if valid */
static void pci_probe_function(uint8_t bus, uint8_t dev,
                               uint8_t func) {
    uint16_t vid = pci_config_read16(bus, dev, func, 0x00);
    if (vid == 0xFFFF) return;           /* no device here */

    if (pci_device_count >= PCI_MAX_DEVICES) return;

    pci_device_t *pd = &pci_devices[pci_device_count++];
    memset(pd, 0, sizeof(*pd));

    pd->bus         = bus;
    pd->device      = dev;
    pd->function    = func;
    pd->vendor_id   = vid;
    pd->device_id   = pci_config_read16(bus, dev, func, 0x02);
    pd->revision_id = pci_config_read8(bus, dev, func, 0x08);

    /* Class/Subclass/ProgIF */
    pd->prog_if     = pci_config_read8(bus, dev, func, 0x09);
    pd->subclass    = pci_config_read8(bus, dev, func, 0x0A);
    pd->class_code  = pci_config_read8(bus, dev, func, 0x0B);

    /* Header type tells us BAR count and if multi-function */
    pd->header_type = pci_config_read8(bus, dev, func, 0x0E);

    /* IRQ routing */
    pd->irq_line    = pci_config_read8(bus, dev, func, 0x3C);
    pd->irq_pin     = pci_config_read8(bus, dev, func, 0x3D);

    /* Enumerate BARs */
    uint8_t max_bars = (pd->header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE
                       ? PCI_NUM_BARS_BRIDGE
                       : PCI_NUM_BARS_DEVICE;

    for (uint8_t bar = 0; bar < max_bars; bar++) {
        uint8_t off = 0x10 + bar * 4;
        uint32_t orig = pci_config_read32(bus, dev, func, off);
        if (orig == 0) continue;

        /* Write all 1s to probe size, then restore */
        pci_config_write32(bus, dev, func, off, 0xFFFFFFFFu);
        uint32_t sz  = pci_config_read32(bus, dev, func, off);
        pci_config_write32(bus, dev, func, off, orig);

        pd->bars[bar].valid       = true;
        pd->bars[bar].mmio        = (orig & 1) == 0;
        pd->bars[bar].prefetchable= !!(orig & (1<<3));

        if (pd->bars[bar].mmio) {
            uint64_t mask = ~(uint64_t)(sz & 0xFFFFFFF0u) + 1;
            pd->bars[bar].size = mask;
            pd->bars[bar].addr = orig & 0xFFFFFFF0u;
        } else {
            uint16_t mask = ~(uint16_t)(sz & 0xFFFFFFFCu) + 1;
            pd->bars[bar].size = mask;
            pd->bars[bar].addr = orig & 0xFFFFFFFCu;
        }
        pd->bar_count++;
    }

    /* If this is a PCI-to-PCI bridge, scan its secondary bus */
    if ((pd->header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {
        uint8_t sec_bus = pci_config_read8(bus, dev, func, 0x19);
        for (uint8_t d = 0; d < 32; d++)
            pci_scan_bus(sec_bus);
    }
}

/* Scan one device slot (function 0 + possible multi-function) */
static void pci_scan_device(uint8_t bus, uint8_t dev) {
    uint8_t hdr = pci_config_read8(bus, dev, 0, 0x0E);
    pci_probe_function(bus, dev, 0);
    if (hdr & PCI_HEADER_MULTIFUNC) {
        for (uint8_t f = 1; f < 8; f++) {
            uint16_t vid = pci_config_read16(bus, dev, f, 0x00);
            if (vid != 0xFFFF) {
                pci_probe_function(bus, dev, f);
            }
        }
    }
}

/* Scan all 32 device numbers on given bus */
static void pci_scan_bus(uint8_t bus) {
    for (uint8_t dev = 0; dev < 32; dev++) {
        pci_scan_device(bus, dev);
    }
}

/* Public API */
void pci_init(void) {
    pci_device_count = 0;
    pci_scan_bus(0);
}

pci_device_t *pci_find_device(uint16_t vendor_id,
                              uint16_t device_id) {
    for (size_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id)
            return &pci_devices[i];
    }
    return NULL;
}
