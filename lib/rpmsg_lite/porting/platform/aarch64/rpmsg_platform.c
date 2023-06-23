/*
 * Copyright 2022-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "rpmsg_platform.h"
#include "rpmsg_env.h"

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

#ifndef RL_GEN_SW_MBOX_BASE
#error "RL_GEN_SW_MBOX_BASE is NOT defined, define it in rpmsg_config.h!"
#endif

#ifndef RL_GEN_SW_MBOX_IRQ
#error "RL_GEN_SW_MBOX_IRQ is NOT defined, define it in rpmsg_config.h!"
#endif

#ifndef RL_GEN_SW_MBOX_REMOTE_IRQ
#error "RL_GEN_SW_MBOX_REMOTE_IRQ is NOT defined, define it in rpmsg_config.h!"
#endif

/*
 * Generic software mailbox Registers:
 *
 * RX_STATUS[n]: RX channel n status
 * TX_STATUS[n]: TX channel n status
 * 	0: indicates message in T/RX_CH[n] is invalid and channel ready.
 * 	1: indicates message in T/RX_CH[n] is valid and channel busy.
 * 	2: indicates message in T/RX_CH[n] has been received by the peer.
 * RX_CH[n]: Receive data register for channel n
 * TX_CH[n]: Transmit data register for channel n
 *
 * To send a message:
 * Update the data register TX_CH[n] with the message, then set the
 * TX_STATUS[n] to 1, inject a interrupt to remote side; after the
 * transmission done set the TX_STATUS[n] back to 0.
 *
 * When received a message:
 * Get the received data from RX_CH[n] and then set the RX_STATUS[n] to
 * 2 and inject a interrupt to notify the remote side transmission done.
 */

#define MAX_CH (4)

struct gen_sw_mbox
{
    uint32_t rx_status[MAX_CH];
    uint32_t tx_status[MAX_CH];
    uint32_t reserved[MAX_CH];
    uint32_t rx_ch[MAX_CH];
    uint32_t tx_ch[MAX_CH];
};

enum sw_mbox_channel_status
{
    S_READY,
    S_BUSY,
    S_DONE,
};

static int32_t disable_counter = 0;
static void *platform_lock;
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
static LOCK_STATIC_CONTEXT platform_lock_static_ctxt;
#endif

void gen_sw_mbox_handler(void *data)
{
    struct gen_sw_mbox *base = (struct gen_sw_mbox *)data;
    uint32_t vector_id;

    if (base->tx_status[RPMSG_MBOX_CHANNEL] == S_DONE)
        base->tx_status[RPMSG_MBOX_CHANNEL] = S_READY;

    /* Check if the interrupt is for us */
    if (base->rx_status[RPMSG_MBOX_CHANNEL] != S_BUSY)
        return;

    vector_id = base->rx_ch[RPMSG_MBOX_CHANNEL];

    base->rx_status[RPMSG_MBOX_CHANNEL] = S_DONE;
    __DSB();
    GIC_SetPendingIRQ(RL_GEN_SW_MBOX_REMOTE_IRQ);

    env_isr(vector_id >> 16);
}

static void gen_sw_mailbox_init(struct gen_sw_mbox *base)
{
    /* Clear status register */
    base->rx_status[RPMSG_MBOX_CHANNEL] = 0;
    base->tx_status[RPMSG_MBOX_CHANNEL] = 0;
}

static void gen_sw_mbox_sendmsg(struct gen_sw_mbox *base, uint32_t ch, uint32_t msg)
{
    while (base->tx_status[ch] != S_READY)
    {
        /* Avoid sending the same vq id multiple times when channel is busy */
        if (msg == base->tx_ch[ch])
            return;
    }

    base->tx_ch[ch]     = msg;
    base->tx_status[ch] = S_BUSY;
    /* sync before trigger interrupt to remote */
    __DSB();

    GIC_SetPendingIRQ(RL_GEN_SW_MBOX_REMOTE_IRQ);
}

static void platform_global_isr_disable(void)
{
    __asm volatile("MSR DAIFSET, #2" ::: "memory");
    __asm volatile("DSB SY");
    __asm volatile("ISB SY");
}

static void platform_global_isr_enable(void)
{
    __asm volatile("MSR DAIFCLR, #2" ::: "memory");
    __asm volatile("DSB SY");
    __asm volatile("ISB SY");
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
    gen_sw_mbox_sendmsg((struct gen_sw_mbox *)RL_GEN_SW_MBOX_BASE, RPMSG_MBOX_CHANNEL, msg);
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
        GIC_EnableIRQ(RL_GEN_SW_MBOX_IRQ);
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
    /* virtqueues use the same GIC vector
       if counter is set - the interrupts are disabled */
    if (disable_counter == 0)
    {
        GIC_DisableIRQ(RL_GEN_SW_MBOX_IRQ);
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
    gen_sw_mailbox_init((struct gen_sw_mbox *)RL_GEN_SW_MBOX_BASE);

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
