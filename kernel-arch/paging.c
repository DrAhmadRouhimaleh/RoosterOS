#include "paging.h"
#include <string.h>

/* Place page directory & first table in identity-mapped physical frames */
pde_t *kernel_page_directory   = (pde_t *)PAGE_DIR_VADDR;
pde_t *current_page_directory  = NULL;

void paging_init(void) {
    /* 1) Allocate & zero a fresh page directory */
    uint32_t pd_phys = alloc_frame();
    memset((void*)(PAGE_DIR_VADDR), 0, PAGE_SIZE);
    kernel_page_directory = (pde_t*)PAGE_DIR_VADDR;

    /* 2) Identity-map first 4 MB with 4 KB pages */
    uint32_t pt_phys = alloc_frame();
    /* map the new table at its virtual slot:
     * PAGE_TABLES_BASE + (0 << 12) => first page table */
    pte_t *first_table = (pte_t*)(PAGE_TABLES_BASE + (0 << 12));
    memset(first_table, 0, PAGE_SIZE);

    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
        first_table[i] = (i * PAGE_SIZE)
                       | PF_PRESENT | PF_RW;
    }
    kernel_page_directory[0] = pt_phys | PF_PRESENT | PF_RW;

    /* 3) Optionally identity-map the rest with 4 MB pages (PSE) */
    for (uint32_t i = 1; i < DIRECTORY_ENTRIES; i++) {
        kernel_page_directory[i]
            = (i * LARGE_PAGE_SIZE)
            | PF_PRESENT
            | PF_RW
            | PF_PAGE_SIZE;
    }

    /* 4) Self-reference the page directory in its last slot */
    kernel_page_directory[DIRECTORY_ENTRIES - 1]
        = pd_phys | PF_PRESENT | PF_RW;

    /* 5) Activate paging */
    switch_page_directory(kernel_page_directory);
}

void switch_page_directory(pde_t *new_directory) {
    current_page_directory = new_directory;
    /* Load physical address of page directory into CR3 */
    uint32_t phys = ((uint32_t)new_directory) - PAGE_DIR_VADDR;
    asm volatile("mov %0, %%cr3" :: "r"(phys) : "memory");
    /* Ensure PSE is enabled (CR4.PSE = bit 4) */
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 4);
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
    /* Finally turn on paging (CR0.PG = bit 31) */
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1u << 31);
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

pte_t *get_pte(uint32_t virt_addr, int create) {
    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;

    /* Access PDE via self-mapped page */
    pde_t *pde = &current_page_directory[pd_index];
    if (!(*pde & PF_PRESENT)) {
        if (!create) return NULL;
        /* Allocate a new page table */
        uint32_t new_pt_phys = alloc_frame();
        memset((void*)(PAGE_TABLES_BASE + (pd_index << 12)), 0, PAGE_SIZE);
        *pde = new_pt_phys | PF_PRESENT | PF_RW | PF_USER;
    }

    /* Return the PTE pointer in virtual space */
    pte_t *table = (pte_t*)(PAGE_TABLES_BASE + (pd_index << 12));
    return &table[pt_index];
}

void map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    pte_t *entry = get_pte(virt_addr, 1);
    *entry = (phys_addr & 0xFFFFF000) | (flags & 0xFFF) | PF_PRESENT;
    flush_tlb(virt_addr);
}

void unmap_page(uint32_t virt_addr) {
    pte_t *entry = get_pte(virt_addr, 0);
    if (!entry || !(*entry & PF_PRESENT)) return;
    *entry = 0;
    flush_tlb(virt_addr);
}
