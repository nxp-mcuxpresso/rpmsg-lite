/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "rpmsg_platform.h"
#include "rpmsg_env.h"
#include "irq.h"
#include "FreeRTOS.h"

#include "fsl_device_registers.h"

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

#ifndef RL_SGIMBOX_BASE
#error "RL_SGIMBOX_BASE is NOT defined, define it in rpmsg_config.h!"
#endif

#ifndef RL_SGIMBOX_TARGET_CORE
/* Define RL_SGIMBOX_TARGET_CORE in applications' rpmsg_config.h, otherwise core0 will be used by default */
#define RL_SGIMBOX_TARGET_CORE	(0)
#endif

#ifndef RL_SGIMBOX_IRQ
/* Define RL_SGIMBOX_IRQ in applications' rpmsg_config.h, otherwise Software10_IRQn will be used by default */
#define RL_SGIMBOX_IRQ	Software10_IRQn
#endif

#define TARGET_CORE	(1 << (RL_SGIMBOX_TARGET_CORE))

/*
 * status register brelf description:
 *
 * bit31                       |bit7            |bit3         bit0
 * ---------------------------------------------------------------
 * |      |      |      |      |  TX_CH status  |  RX_CH status  |
 * ---------------------------------------------------------------
 * TX_CH field: Each bit for a TX channel
 * RX_CH field: Each bit for a RX channel
 *
 * Set by the initiator to indicate the 'data' in the TX_CH register valid;
 * And clear by the terminator to indicate TX done.
 */

#define MAX_CH		(4)
#define RX_CH_SHFIT	(0)
#define TX_CH_SHFIT	MAX_CH
#define RX_CH_BIT(n)	(1 << (n + RX_CH_SHFIT))
#define TX_CH_BIT(n)	(1 << (n + TX_CH_SHFIT))

struct sgi_mbox {
    uint32_t status;
    uint32_t rx_ch[MAX_CH];
    uint32_t tx_ch[MAX_CH];
};

extern uint64_t ullPortInterruptNesting;

static int32_t disable_counter = 0;
static void *platform_lock;
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
static LOCK_STATIC_CONTEXT platform_lock_static_ctxt;
#endif

static void sgi_mbox_handler(void *data)
{
    struct sgi_mbox *base = (struct sgi_mbox *)data;
    uint32_t vector_id;

    /* Check if the interrupt is for us */
    if ((base->status & RX_CH_BIT(RPMSG_SGI_MBOX_CHANNEL)) == 0)
        return;

    vector_id = base->rx_ch[RPMSG_SGI_MBOX_CHANNEL];

    base->status &= ~(RX_CH_BIT(RPMSG_SGI_MBOX_CHANNEL));
    __DSB();

    env_isr(vector_id >> 16);
}

static void sgi_mailbox_init(struct sgi_mbox *base)
{
    /* Clear status register */
    base->status = 0;
    irq_register(RL_SGIMBOX_IRQ, sgi_mbox_handler, base, (portLOWEST_USABLE_INTERRUPT_PRIORITY - 1) << portPRIORITY_SHIFT);
    GIC_EnableIRQ(RL_SGIMBOX_IRQ);
}

static void sgi_mbox_sendmsg(struct sgi_mbox *base, uint32_t ch, uint32_t msg)
{
    while (base->status & TX_CH_BIT(ch)) {
	/* Avoid sending the same vq id multiple times when channel is busy */
        if (msg == base->tx_ch[ch])
		return;
    }

    base->tx_ch[ch] = msg;
    base->status |= TX_CH_BIT(ch);
    /* sync before trigger SGI */
    __DSB();

    /* trigger SGI to core 0 */
    gic_send_sgi(0, TARGET_CORE, RL_SGIMBOX_IRQ);
}

static void platform_global_isr_disable(void)
{
    __asm volatile ( "MSR DAIFSET, #2" ::: "memory" );
    __asm volatile ( "DSB SY" );
    __asm volatile ( "ISB SY" );
}

static void platform_global_isr_enable(void)
{
    __asm volatile ( "MSR DAIFCLR, #2" ::: "memory" );
    __asm volatile ( "DSB SY" );
    __asm volatile ( "ISB SY" );
}

int32_t platform_init_interrupt(uint32_t vector_id, void *isr_data)
{
    env_lock_mutex(platform_lock);
    /* Register ISR to environment layer */
    env_register_isr(vector_id, isr_data);
    env_unlock_mutex(platform_lock);

    return 0;
}

int32_t platform_deinit_interrupt(uint32_t vector_id)
{
    env_lock_mutex(platform_lock);
    /* Unregister ISR from environment layer */
    env_unregister_isr(vector_id);
    env_unlock_mutex(platform_lock);

    return 0;
}

void platform_notify(uint32_t vector_id)
{
    uint32_t msg = (uint32_t)(vector_id << 16);

    env_lock_mutex(platform_lock);
    sgi_mbox_sendmsg((struct sgi_mbox *)RL_SGIMBOX_BASE, RPMSG_SGI_MBOX_CHANNEL, msg);
    env_unlock_mutex(platform_lock);
}

/**
 * platform_time_delay
 *
 * @param num_msec Delay time in ms.
 *
 * This is not an accurate delay, it ensures at least num_msec passed when return.
 */
void platform_time_delay(uint32_t num_msec)
{
    uint32_t loop;

    /* Recalculate the CPU frequency */
    SystemCoreClockUpdate();

    /* Calculate the CPU loops to delay, each loop has 3 cycles */
    loop = SystemCoreClock / 3U / 1000U * num_msec;

    /* There's some difference among toolchains, 3 or 4 cycles each loop */
    while (loop > 0U)
    {
        __NOP();
        loop--;
    }
}

/**
 * platform_in_isr
 *
 * Return whether CPU is processing IRQ
 *
 * @return True for IRQ, false otherwise.
 *
 */
int32_t platform_in_isr(void)
{
    /* This is only working for FreeRTOS */
    return (ullPortInterruptNesting > 0);
}

/**
 * platform_interrupt_enable
 *
 * Enable peripheral-related interrupt
 *
 * @param vector_id Virtual vector ID that needs to be converted to IRQ number
 *
 * @return vector_id Return value is never checked.
 *
 */
int32_t platform_interrupt_enable(uint32_t vector_id)
{
    RL_ASSERT(0 < disable_counter);

    platform_global_isr_disable();
    disable_counter--;

    if (disable_counter == 0)
    {
	GIC_EnableIRQ(RL_SGIMBOX_IRQ);
    }
    platform_global_isr_enable();

    return ((int32_t)vector_id);
}

/**
 * platform_interrupt_disable
 *
 * Disable peripheral-related interrupt.
 *
 * @param vector_id Virtual vector ID that needs to be converted to IRQ number
 *
 * @return vector_id Return value is never checked.
 *
 */
int32_t platform_interrupt_disable(uint32_t vector_id)
{
    RL_ASSERT(0 <= disable_counter);

    platform_global_isr_disable();
    /* virtqueues use the same NVIC vector
       if counter is set - the interrupts are disabled */
    if (disable_counter == 0)
    {
	GIC_DisableIRQ(RL_SGIMBOX_IRQ);
    }
    disable_counter++;
    platform_global_isr_enable();

    return ((int32_t)vector_id);
}

/**
 * platform_map_mem_region
 *
 * Dummy implementation
 *
 */
void platform_map_mem_region(uint32_t vrt_addr, uint32_t phy_addr, uint32_t size, uint32_t flags)
{
}

/**
 * platform_cache_all_flush_invalidate
 *
 * Dummy implementation
 *
 */
void platform_cache_all_flush_invalidate(void)
{
}

/**
 * platform_cache_disable
 *
 * Dummy implementation
 *
 */
void platform_cache_disable(void)
{
}

/**
 * platform_vatopa
 *
 * Dummy implementation
 *
 */
uintptr_t platform_vatopa(void *addr)
{
    return ((uintptr_t)(char *)addr);
}

/**
 * platform_patova
 *
 * Dummy implementation
 *
 */
void *platform_patova(uintptr_t addr)
{
    return ((void *)(char *)addr);
}

/**
 * platform_init
 *
 * platform/environment init
 */
int32_t platform_init(void)
{
    sgi_mailbox_init((struct sgi_mbox *)RL_SGIMBOX_BASE);

    /* Create lock used in multi-instanced RPMsg */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
    if (0 != env_create_mutex(&platform_lock, 1, &platform_lock_static_ctxt))
#else
    if (0 != env_create_mutex(&platform_lock, 1))
#endif
    {
        return -1;
    }

    return 0;
}

/**
 * platform_deinit
 *
 * platform/environment deinit process
 */
int32_t platform_deinit(void)
{
    /* Delete lock used in multi-instanced RPMsg */
    env_delete_mutex(platform_lock);
    platform_lock = ((void *)0);

    return 0;
}
