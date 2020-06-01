#include "device/tty.h"
#include "multiboot.h"
#include "tar.h"
#include "kdebug.h"
#include "mem.h"
#include "arch/cpu.h"
#include "arch/paging.h"
#include "syscall.h"
#include "proc.h"
#include "sync.h"
#include <stddef.h>
#include <string.h>
#include "device/initrd.h"
#include "device.h"
#include "fs.h"
#include "fs/tarfs.h"



void kmain(unsigned long magic, unsigned long addr) {
    acquire_global();

    tty_init();
    init_cpu();
    kassert(magic == MULTIBOOT_BOOTLOADER_MAGIC);

    multiboot_info_t *mbi = (multiboot_info_t *) addr;

    multiboot_module_t *mods = (multiboot_module_t *) mbi->mods_addr;

    kassert(mbi->mods_count == 1);

    kprintf("found initramfs '%s' at %X...%X\n",
            (const char *) mods[0].cmdline,
            mods[0].mod_start, mods[0].mod_end);

    kprintf("loading initrd\n");
    initrd_init((void *)mods[0].mod_start, (void *)mods[0].mod_end);

    multiboot_memory_map_t *mems = (multiboot_memory_map_t *) mbi->mmap_addr;
    multiboot_memory_map_t *mend = ((void *) mems) + mbi->mmap_length;

    multiboot_memory_map_t *m;
    for (m = mems; m < mend; m = ((void *) m) + m->size + 4) {
        if (m->addr == 0x100000) {
            // we only use this region for now.

            //TODO: detect initbrk better.
            kprintf("initializing memory manager\n");
            kprintf("mem: %llX...%llX\n", m->addr, m->addr + m->len);
            mem_init(m->addr, mods[0].mod_end, m->len);
            kprintf("%lluMB free\n", m->len / (1024 * 1024));
            break;
        }
    }


    release_global();

    struct inode in = {
        .ftype = FT_SPECIAL,
        .dev = MKDEV(DEV_INITRD, 0),
    };

    fs_mount("/", &in, get_tarfs());

    init_proc();

    // here we go!
    sys_exec("/bin/init");
}
