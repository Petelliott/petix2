#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

typedef uint32_t page_t;

// currently only continuous ram can be used
void mem_init(uintptr_t base, uintptr_t initbrk, uintptr_t length);

// don't call these directly
int kbrk(void *addr);
void *ksbrk(intptr_t increment);

page_t alloc_page(void);
void free_page(page_t page);

#endif