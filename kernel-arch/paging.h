#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/* Page sizes & counts */
#define PAGE_SIZE            4096
#define PAGE_ENTRIES         1024
#define DIRECTORY_ENTRIES    1024
#define LARGE_PAGE_SIZE      (4 * 1024 * 1024)

/* Virtual addresses for self-mapping */
#define PAGE_DIR_VADDR       0xFFFFF000   /* last PDE points back to directory */
#define PAGE_TABLES_BASE     0xFFC00000   /* maps all 1024 tables in one block */

/* Page/Directory entry flags */
#define PF_PRESENT     0x001    /* must be 1 to be valid */
#define PF_RW          0x002    /* 0 = read-only; 1 = read/write */
#define PF_USER        0x004    /* 0 = kernel-only; 1 = user-mode */
#define PF_PWT         0x008    /* write-through */
#define PF_PCD         0x010    /* cache disable */
#define PF_ACCESSED    0x020    /* CPU sets on read/write */
#define PF_DIRTY       0x040    /* CPU sets on write (PTE only) */
#define PF_PAGE_SIZE   0x080    /* PDE: 0=4 KB pages; 1=4 MB pages */
#define PF_GLOBAL      0x100    /* ignored unless CR4.PGE=1 */

/* Types */
typedef uint32_t pde_t;
typedef uint32_t pte_t;

/* Externally provided frame allocator (physical frames) */
extern uint32_t alloc_frame(void);
extern void     free_frame(uint32_t frame_addr);

/* Kernel / current page directories */
extern pde_t *kernel_page_directory;
extern pde_t *current_page_directory;

/* Initialize paging (identity-map + PSE + self-map) */
void paging_init(void);

/* Switch to a new page directory (loads CR3) */
void switch_page_directory(pde_t *new_directory);

/* Obtain pointer to a PTE for a virtual address; create if requested */
pte_t *get_pte(uint32_t virt_addr, int create);

/* Map or unmap a single 4 KB page */
void  map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
void  unmap_page(uint32_t virt_addr);

/* Flush a single page from TLB */
static inline void flush_tlb(uint32_t virt_addr) {
    asm volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

#endif /* PAGING_H */
