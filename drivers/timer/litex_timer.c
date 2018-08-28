/*
 * Copyright (c) 2018 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <arch/cpu.h>
#include <device.h>
#include <system_timer.h>
#include <misc/printk.h>

#define TIMER_BASE                  CONFIG_TIMER_0_BASE_ADDR
#define TIMER_LOAD_ADDR             (TIMER_BASE)
#define TIMER_RELOAD_ADDR           ((TIMER_BASE) + 0x10)
#define TIMER_EN_ADDR               ((TIMER_BASE) + 0x20)
#define TIMER_UPDATE_VALUE_ADDR     ((TIMER_BASE) + 0x24)
#define TIMER_VALUE_ADDR            ((TIMER_BASE) + 0x28)
#define TIMER_EV_STATUS_ADDR        ((TIMER_BASE) + 0x38)
#define TIMER_EV_PENDING_ADDR       ((TIMER_BASE) + 0x3c)
#define TIMER_EV_ENABLE_ADDR        ((TIMER_BASE) + 0x40)

#define TIMER_IRQ                   CONFIG_TIMER_0_IRQ_0
#define TIMER_VALUE                 1000000

#define REG_READ(reg)               (*(volatile u32_t*)(reg))
#define REG_WRITE(reg, val)         (REG_READ(reg) = (u32_t)val)


static u32_t accumulated_cycle_count;

static void litex_timer_irq_handler(void *device)
{
    ARG_UNUSED(device);
    REG_WRITE(TIMER_EV_PENDING_ADDR, 1);

    accumulated_cycle_count += sys_clock_hw_cycles_per_tick();

    z_clock_announce(1);
}

u32_t _timer_cycle_get_32(void)
{
    REG_WRITE(TIMER_UPDATE_VALUE_ADDR, 1);

    return accumulated_cycle_count;
}

u32_t z_clock_elapsed(void)
{
    return 0;
}

int z_clock_driver_init(struct device *device)
{
    ARG_UNUSED(device);
    IRQ_CONNECT(TIMER_IRQ, 0, litex_timer_irq_handler, NULL, 0);
    irq_enable(TIMER_IRQ);

    REG_WRITE(TIMER_EN_ADDR, 0);

    for (int i = 0; i < 4; i++) {
        REG_WRITE(TIMER_RELOAD_ADDR + i*0x4, TIMER_VALUE >> (24 - i*8));
        REG_WRITE(TIMER_LOAD_ADDR + i*0x4, TIMER_VALUE >> (24 - i*8));
    }

    REG_WRITE(TIMER_EN_ADDR, 1);

    REG_WRITE(TIMER_EV_PENDING_ADDR, REG_READ(TIMER_EV_PENDING_ADDR));
    REG_WRITE(TIMER_EV_ENABLE_ADDR, 1);

    return 0;
}
