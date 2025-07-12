# RoosterOS
# 🐓 RoosterOS — The Bare-Metal Phoenix

**RoosterOS** is a modular operating system designed from scratch to explore the art and science of low-level systems. This repo simulates its early boot sequence using Python — providing clarity, flexibility, and insight into how x86 machines begin their journey from BIOS to kernel.

---

## 🌟 Philosophy & Goals

RoosterOS isn't just another hobby kernel. It's built around these principles:

- 🧵 **Hardware intimacy** — No abstraction without understanding
- 🧱 **Modularity first** — Each subsystem is structured, testable, and extensible
- 📐 **Architectural scalability** — Designed to scale from legacy x86 to modern ARM/RISC-V
- 🎓 **Educational clarity** — Built to teach, not just to work
- 🖥️ **Practical usability** — Intuitive install and boot, support for GRUB/UEFI, and custom file formats
- 🪶 **Clean boot philosophy** — From segment reloads to page tables, every detail is intentional

Whether you're writing your first operating system or dreaming of your twentieth, RoosterOS is meant to be a foundation — both technically and conceptually.

---

## 🔍 What's Simulated Here

All code is in Python, organized into modular components. Each simulates a specific phase or hardware feature in the boot process.

| File                      | Description                                                                 |
|---------------------------|-----------------------------------------------------------------------------|
| `realmode_sim.py`         | 16-bit real-mode stub: segment setup, A20 gate, GDT load, far jump          |
| `protectedmode_sim.py`    | Protected-mode initialization: GDT, IDT, paging, CPU features, kernel handoff |
| `gdt.py`                  | Builds and simulates loading of Global Descriptor Table (GDT)               |
| `idt.py`                  | Builds Interrupt Descriptor Table (IDT), dispatches traps and IRQs          |
| `cpu.py`                  | Simulates CR0/CR4 and enables SSE/NX via MSR                                |
| `paging.py`               | Two-level paging with 4K and 4M support, self-map, TLB flush                 |
| `pic.py`                  | PIC remapping, masking, EOI, IRQ dispatch                                   |
| `multiboot_info.py`       | Parses and inspects Multiboot-compliant loader info                         |
| `console.py`              | Console and serial output, including memory map visualization               |
| `kernel_main.py`          | Entry point to the simulated kernel: scheduler, init tasks, memory manager |
| `link_boot.py`            | Python-based boot image linker with alignment, padding, signature injection |
| `entry.py`                | Emulates final boot entry: segment reloads, paging, PIC, handoff            |

All files log their activity at runtime so you can follow every step of the boot process.

---
Why RoosterOS Matters
Operating systems are often black boxes — bootloaders even more so. RoosterOS treats early initialization as a craft: visible, inspectable, and extensible. The goal is to build an OS not just for performance, but for understanding.
By using Python to express low-level simulation, we make boot concepts tangible and testable — no hardware or hypervisor required.

License & Credits
This simulation project is created with the help of Copilot AI, in collaboration with human design and engineering.

RoosterOS is being developed in two core languages:

🐍 Python — used to simulate architectural flows, debugging, and teaching

⚙️ Assembly / C — used to build the actual bare-metal operating system

This dual-language approach allows RoosterOS to function both as an educational simulator and as a real, bootable OS.

🚫 Commercial use  is prohibited.

Upon completion, RoosterOS will support customization and openness similar to Linux — giving developers freedom to tailor builds and behaviors, while preserving the system’s design integrity.



