/*
 * Copyright 2022-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "rpmsg_env.h"
#include "rpmsg_platform.h"
#include "fsl_device_registers.h"
#include "gen_sw_mbox.h"
#include "gen_sw_mbox_config.h"

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

static int32_t disable_counter = 0;
static void *platform_lock;
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
static LOCK_STATIC_CONTEXT platform_lock_static_ctxt;
#endif

#ifndef GEN_SW_MBOX_BASE
#error "GEN_SW_MBOX_BASE is NOT defined!"
#endif

static void mbox_recv_cb(void *data, uint32_t msg)
{
    uint32_t vector_id = msg >> 16;

    env_isr(vector_id);
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
    gen_sw_mbox_sendmsg((void *)GEN_SW_MBOX_BASE, RPMSG_MBOX_CHANNEL, msg, true);
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

    /* application should enable the generic mbox interrupt line */
    if (disable_counter == 0)
    {
	/* do nothing */;
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
    /* we only use one channel, so don't disable generic mbox interrupt line */
    if (disable_counter == 0)
    {
	/* do nothing */;
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
    /* Create lock used in multi-instanced RPMsg */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
    if (0 != env_create_mutex(&platform_lock, 1, &platform_lock_static_ctxt))
#else
    if (0 != env_create_mutex(&platform_lock, 1))
#endif
    {
        return -1;
    }

    gen_sw_mbox_register_chan_callback((void *)GEN_SW_MBOX_BASE, RPMSG_MBOX_CHANNEL, mbox_recv_cb, NULL);

    return 0;
}

/**
 * platform_deinit
 *
 * platform/environment deinit process
 */
int32_t platform_deinit(void)
{
    gen_sw_mbox_unregister_chan_callback((void *)GEN_SW_MBOX_BASE, RPMSG_MBOX_CHANNEL);

    /* Delete lock used in multi-instanced RPMsg */
    env_delete_mutex(platform_lock);
    platform_lock = ((void *)0);

    return 0;
}
