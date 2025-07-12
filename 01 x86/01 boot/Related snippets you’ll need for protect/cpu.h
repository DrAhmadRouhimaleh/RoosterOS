// cpu.h
static inline void cpu_halt(void) {
    for (;; ) __asm__ volatile ("hlt");
}
void cpu_enable_sse(void);
void cpu_enable_nx(void);
