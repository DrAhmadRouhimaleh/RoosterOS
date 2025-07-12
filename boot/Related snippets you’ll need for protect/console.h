// console.h
void console_init(void);
void console_clear(void);
void console_puts(const char *s);
void console_puthex(uint32_t v);
void serial_init(uint32_t baud);
void print_memory_map(uint32_t addr, uint32_t len);
