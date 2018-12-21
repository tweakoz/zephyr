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
#define IRQ_MASK    CONFIG_LITEX_PICORV32_INTC_0_IRQ_MASK
#define IRQ_PENDING CONFIG_LITEX_PICORV32_INTC_0_IRQ_PENDING

#define ILLEGAL_INSTRUCTION_ERROR 1
#define BUS_ERROR 2
#define TIMER_INTERRUPT 4
#define UART_INTERRUPT 5

void _irq_enable(void);
void _irq_disable(void);
void _irq_setmask(u32_t mask);
u32_t _irq_pending(void);
extern u32_t _irq_mask;

static inline void picorv32_litex_irq_setmask(u32_t mask)
{
    _irq_setmask(~mask);
}

static inline u32_t picorv32_litex_irq_getmask(void)
{
    return ~_irq_mask;
}

static inline u32_t picorv32_litex_irq_pending(void)
{
    return _irq_pending();
}

static inline void picorv32_litex_irq_setie(u32_t ie)
{
    if (ie & 0x1)
        _irq_enable();
    else
        _irq_disable();
}

void _arch_irq_enable(unsigned int irq)
{
    picorv32_litex_irq_setmask(picorv32_litex_irq_getmask() | (1 << irq));
}

void _arch_irq_disable(unsigned int irq)
{
    picorv32_litex_irq_setmask(picorv32_litex_irq_getmask() & ~(1 << irq));
}

int _arch_irq_is_enabled(unsigned int irq)
{
    return picorv32_litex_irq_getmask() & (1 << irq);
}

static int picorv32_litex_irq_init(struct device *dev)
{
    ARG_UNUSED(dev);

    irq_enable(ILLEGAL_INSTRUCTION_ERROR);
    irq_enable(BUS_ERROR);
    return 0;
}

SYS_INIT(picorv32_litex_irq_init, PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
