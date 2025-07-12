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

RoosterOS is a modular, multi-language operating system that targets both real-world performance and educational clarity. The actual kernel is written in Assembly, C, Rust, and Go to deliver low-level control, safety, and speed, while a parallel Python implementation simulates every step of the x86 boot process—from real-mode segment setup and A20 gate to GDT/IDT loading, paging, and protected-mode handoff—enabling rapid prototyping, testing, and learning.

The project’s design and documentation have been enriched with the assistance of Copilot AI, ensuring consistent structure and thorough coverage of each subsystem. All code and simulation components are provided for personal and educational use only; commercial use of RoosterOS in any form is strictly prohibited. 

Our primary goal is to design an operating system that spans the spectrum from legacy 16-bit machines to modern hardware, serving as a viable alternative to Windows. It will feature a clean, user-friendly graphical interface that performs smoothly even on 16-bit platforms or systems with as little as 64 MB of RAM.

We plan to support a wide array of languages—including English, Persian, Chinese, Japanese, Arabic, Korean, Russian, German, Spanish, Italian, French, Somali, Berber, Amharic, Oromo, Igbo, Swahili, Hausa, Mandinka, Fulani, many other African languages, and more—though initially we will implement full support for English, Persian, Arabic, and possibly Chinese.
