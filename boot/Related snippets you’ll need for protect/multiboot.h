// multiboot.h
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
typedef struct {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
    uint32_t mmap_length, mmap_addr;
    /* … */
} multiboot_info_t;
