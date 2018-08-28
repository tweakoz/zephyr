/*
 * Copyright (c) 2018 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <arch/cpu.h>
#include <init.h>
#include <irq.h>
#include <device.h>

#include <zephyr.h>
#include <zephyr/types.h>
#include <misc/printk.h>

#define MSTATUS_MIE SOC_MSTATUS_IEN
#define IRQ_MASK    CONFIG_LITEX_VEXRISCV_INTC_0_IRQ_MASK
#define IRQ_PENDING CONFIG_LITEX_VEXRISCV_INTC_0_IRQ_PENDING

#define TIMER_INTERRUPT 1
#define UART_INTERRUPT 2

static inline void vexriscv_litex_irq_setmask(u32_t mask)
{
    __asm__ volatile ("csrw %0, %1" :: "i"(IRQ_MASK), "r"(mask));
}

static inline u32_t vexriscv_litex_irq_getmask(void)
{
    u32_t mask;
    __asm__ volatile ("csrr %0, %1" : "=r"(mask) : "i"(IRQ_MASK));
    return mask;
}

static inline u32_t vexriscv_litex_irq_pending(void)
{
    u32_t pending;
    __asm__ volatile ("csrr %0, %1" : "=r"(pending) : "i"(IRQ_PENDING));
    return pending;
}

static inline void vexriscv_litex_irq_setie(u32_t ie)
{
    if (ie)
        __asm__ volatile ("csrrs x0, mstatus, %0" :: "r"(MSTATUS_MIE));
    else
        __asm__ volatile ("csrrc x0, mstatus, %0" :: "r"(MSTATUS_MIE));
}

static void vexriscv_litex_irq_handler(void *device) {
    struct _isr_table_entry *ite;
    u32_t irqs = vexriscv_litex_irq_pending() & vexriscv_litex_irq_getmask();

#ifdef CONFIG_LITEX_TIMER
    if (irqs & (1 << TIMER_INTERRUPT)) {
        ite = (struct _isr_table_entry*)&_sw_isr_table[TIMER_INTERRUPT];
        ite->isr(NULL);
    }
#endif

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
    if (irqs & (1 << UART_INTERRUPT)) {
        ite = (struct _isr_table_entry*)&_sw_isr_table[UART_INTERRUPT];
        ite->isr(ite->arg);
    }
#endif
}

void _arch_irq_enable(unsigned int irq) {
    vexriscv_litex_irq_setmask(vexriscv_litex_irq_getmask() | (1 << irq));
}

void _arch_irq_disable(unsigned int irq) {
    vexriscv_litex_irq_setmask(vexriscv_litex_irq_getmask() & ~(1 << irq));
}

int _arch_irq_is_enabled(unsigned int irq) {
    return vexriscv_litex_irq_getmask() & (1 << irq);
}

static int vexriscv_litex_irq_init(struct device *dev)
{
    ARG_UNUSED(dev);
    vexriscv_litex_irq_setie(1);
    IRQ_CONNECT(RISCV_MACHINE_EXT_IRQ, 0, vexriscv_litex_irq_handler, NULL, 0);

    return 0;
}

SYS_INIT(vexriscv_litex_irq_init, PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
