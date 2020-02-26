#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>
#include "../cpu.h"


struct pushed_regs {
    int32_t  vecn;
    int32_t  exception;
    int32_t  irq;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t error_code; // undefined value if there is no error code.
} __attribute__((packed));

typedef void(*interrupt_handler_t)(int vecn, int exception, int irq);

void send_eoi(int irq);

void clear_interrupt_handlers(void);

void register_interrupt_handler(int vecn, interrupt_handler_t handler);

void general_interrupt_handler(struct pushed_regs regs);

void PIC_remap(int offset1, int offset2);

#endif