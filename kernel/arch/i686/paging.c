#include "../paging.h"
#include "mmu.h"
#include "interrupts.h"
#include "../../kdebug.h"
#include "../../mem.h"
#include <stddef.h>
#include <string.h>

#define PROC_REGION 0xC0000000

static page_dir_t kpagedir;

#define identity_len ((PROC_REGION/PAGE_SIZE)/PTAB_SIZE)

static page_tab_t page_tabs[identity_len];

static void page_fault_handler(struct pushed_regs *regs);

void init_paging(void) {
    kassert(sizeof(struct page_tab_ent) == 4);
    kassert(sizeof(struct page_dir_ent) == 4);
    kassert((((uint32_t) kpagedir.ents) & 0xfff) == 0);
    kassert((((uint32_t) page_tabs) & 0xfff) == 0);

    memset(&kpagedir, 0, sizeof(kpagedir));
    memset(&page_tabs, 0, sizeof(page_tabs));

    // identity page up to 0xc000000
    for (size_t i = 0; i < identity_len; ++i) {
        kpagedir.ents[i].present    = 1;
        kpagedir.ents[i].rw         = 1;
        kpagedir.ents[i].page_table = ((uint32_t) page_tabs[i].ents) >> PAGE_SHIFT;

        for (size_t j = 0; j < PTAB_SIZE; ++j) {
            struct page_tab_ent *pte = &(page_tabs[i].ents[j]);
            pte->present = 1;
            pte->rw = 1;
            pte->global = 1;
            pte->addr = (i*(PAGE_SIZE*PTAB_SIZE) + j*PAGE_SIZE) >> PAGE_SHIFT;
        }
    }

    //unmap the zeropage
    struct page_tab_ent *pte = (void *)(kpagedir.ents[0].page_table << PAGE_SHIFT);
    pte[0].present = 0;

    register_interrupt_handler(14, page_fault_handler);

    load_page_dir(kpagedir.ents);
    enable_paging();
    enable_global_pages();

    kprintf("paging initialized\n");
}

static void page_fault_handler(struct pushed_regs *regs) {
    acquire_global();

    struct page_dir_ent *pd = get_page_dir();
    uintptr_t linaddr;
    asm ("mov %%cr2, %0": "=r" (linaddr));

    //kprintf("page fault: pfla=%lx, ec=%lx\n",
    //        linaddr, regs->error_code);

    if ((regs->error_code & 0x9) != 0) {
        kprintf("unrecoverable page fault. pfla=%lx, ec=%lx\n",
                linaddr, regs->error_code);
        panic("unrecoverable page fault");
    }

    uint32_t dir_idx, tab_idx;
    split_addr(linaddr, dir_idx, tab_idx);

    if (!pd[dir_idx].present && !pd[dir_idx].petix_alloc) {
        kprintf("this should be a segfault (dir). pfla=%lx, ec=%lx, %%eip=%lx\n",
                linaddr, regs->error_code, regs->eip);
        panic("unrecoverable page fault");
    }

    // allocate a page table if needed
    if (!pd[dir_idx].present && pd[dir_idx].petix_alloc) {
        struct page_tab_ent *tab = alloc_page_ptr();
        memset(tab, 0, PAGE_SIZE);
        for (size_t i = 0; i < PTAB_SIZE; ++i) {
            tab[i].petix_alloc = 1;
            tab[i].present = 0;
            tab[i].rw   = pd[dir_idx].rw;
            tab[i].user = pd[dir_idx].user;
        }
        pd[dir_idx].present = 1;
        pd[dir_idx].page_table = (uint32_t) tab >> PAGE_SHIFT;
    }

    // allocate a page if needed

    kassert(pd[dir_idx].present);

    struct page_tab_ent *tab = (void *) (pd[dir_idx].page_table << 12);

    if (!tab[tab_idx].present && !tab[tab_idx].petix_alloc) {
        kprintf("this should be a segfault (page). pfla=%lx, ec=%lx, %%eip=%lx\n",
                linaddr, regs->error_code, regs->eip);
        panic("unrecoverable page fault");
    }

    if (!tab[tab_idx].present && tab[tab_idx].petix_alloc) {
        void *page = alloc_page_ptr();
        memset(page, 0, PAGE_SIZE);
        tab[tab_idx].present = 1;
        tab[tab_idx].global = 0;
        tab[tab_idx].addr = (uint32_t) page >> PAGE_SHIFT;
    }

    // TODO: figure out if we need this
    // flush_tlb();

    release_global();
}

addr_space_t create_proc_addr_space(void) {
    struct page_dir_ent *pd = alloc_page_ptr_sync();
    memcpy(pd, kpagedir.ents, PAGE_SIZE);

    // null initialized pages above to 0xc000000
    for (size_t i = identity_len; i < PDIR_SIZE; ++i) {
        pd[i].present     = 0;
        pd[i].rw          = 1;
        pd[i].user        = 1;
        pd[i].petix_alloc = 1;
    }

    return pd;
}

void free_proc_addr_space(addr_space_t as) {
    if (as == NULL) {
        return;
    }

    for (size_t i = identity_len; i < PDIR_SIZE; ++i) {
        if (as[i].present && as[i].petix_alloc) {
            struct page_tab_ent *pte = (void *) (as[i].page_table << PAGE_SHIFT);
            for (size_t j = 0; j < PTAB_SIZE; ++j) {
                if (pte[j].present && pte[j].petix_alloc) {
                    free_page_sync(pte[j].addr);
                }
            }

            free_page_sync(as[i].page_table);
        }
    }
    free_page_ptr_sync(as);
}

addr_space_t fork_proc_addr_space(addr_space_t as) {
    acquire_global();

    struct page_dir_ent *dir = alloc_page_ptr();
    memcpy(dir, as, PAGE_SIZE);

    for (size_t i = identity_len; i < PDIR_SIZE; ++i) {
        if (as[i].present && as[i].petix_alloc) {
            struct page_tab_ent *old_tab =
                (void *) (as[i].page_table << PAGE_SHIFT);
            struct page_tab_ent *tab = alloc_page_ptr();
            memcpy(tab, old_tab, PAGE_SIZE);

            dir[i].page_table = (uintptr_t) tab >> PAGE_SHIFT;

            for (size_t j = 0; j < PTAB_SIZE; ++j) {
                if (tab[j].present && tab[j].petix_alloc) {
                    void *oldpage = (void *) (tab[j].addr << PAGE_SHIFT);
                    void *page = alloc_page_ptr();

                    memcpy(page, oldpage, PAGE_SIZE);
                    tab[j].addr = (uintptr_t) page >> PAGE_SHIFT;
                }
            }
        }
    }

    release_global();
    return dir;
}

void use_addr_space(addr_space_t as) {
    load_page_dir(as);
}

void lock_page(addr_space_t as, void *addr) {
    uintptr_t dir_idx, tab_idx;
    split_addr((uintptr_t)addr, dir_idx, tab_idx);

    kassert(as[dir_idx].present);
    struct page_tab_ent *tab = (void *) (as[dir_idx].page_table << 12);

    kassert(tab[tab_idx].present);
    tab[tab_idx].user = 0;

}

int remap_page_user(addr_space_t as, void *virt, void *phys) {
    if (virt < (void*)PROC_REGION || (char *)virt > USER_STACK_TOP) {
        return -1;
    }

    // force page table to be loaded for virt
    // TODO: avoid extra allocation
    volatile char a = *(char *) virt;
    a = a;

    uintptr_t dir_idx, tab_idx;
    split_addr((uintptr_t)virt, dir_idx, tab_idx);

    kassert(as[dir_idx].present);
    struct page_tab_ent *tab = (void *) (as[dir_idx].page_table << 12);

    kassert(tab[tab_idx].present);

    if (tab[tab_idx].present && tab[tab_idx].petix_alloc) {
        free_page_sync(tab[tab_idx].addr);
        tab[tab_idx].petix_alloc = false;
    }

    tab[tab_idx].addr = (uintptr_t) phys >> 12;
    return 0;
}

void remap_page_kernel(void *virt, void *phys) {
    uintptr_t dir_idx, tab_idx;
    split_addr((uintptr_t)virt, dir_idx, tab_idx);

    kassert(kpagedir.ents[dir_idx].present);
    struct page_tab_ent *tab = (void *) (kpagedir.ents[dir_idx].page_table << 12);

    kassert(tab[tab_idx].present);

    if (tab[tab_idx].present && tab[tab_idx].petix_alloc) {
        free_page_sync(tab[tab_idx].addr);
        tab[tab_idx].petix_alloc = false;
    }

    tab[tab_idx].addr = (uintptr_t) phys >> 12;
}
