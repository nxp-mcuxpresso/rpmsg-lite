/*
 * Copyright 2024-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "rpmsg_platform.h"
#include "rpmsg_env.h"
#include <xtensa/config/core.h>

#ifdef SDK_OS_BAREMETAL
#include <xtensa/xtruntime.h>
#include <xtensa/tie/xt_interrupt.h>
#else
#include <xtensa/xos.h>
#endif

#include "fsl_device_registers.h"
#include "fsl_mu.h"

#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
#include "mcmgr.h"
#endif

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

static int32_t isr_counter     = 0;
static int32_t disable_counter = 0;
static void *platform_lock;
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
static LOCK_STATIC_CONTEXT platform_lock_static_ctxt;
#endif

#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
static void mcmgr_event_handler(mcmgr_core_t coreNum, uint16_t vring_idx, void *context)
{
    env_isr((uint32_t)vring_idx);
}

#else
void MU3_B_IRQHandler(void *arg)
{
    uint32_t flags;
    flags = MU_GetStatusFlags(MU3_MUB);
    if (((uint32_t)kMU_GenInt0Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(MU3_MUB, (uint32_t)kMU_GenInt0Flag);
        env_isr(RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID << 3U);
    }
    if (((uint32_t)kMU_GenInt1Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(MU3_MUB, (uint32_t)kMU_GenInt1Flag);
        env_isr((uint32_t)(0x01U | (RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID << 3U)));
    }
}
#endif

int32_t platform_init_interrupt(uint32_t vector_id, void *isr_data)
{
    if (platform_lock != ((void *)0))
    {
        /* Register ISR to environment layer */
        env_register_isr(vector_id, isr_data);

        env_lock_mutex(platform_lock);

        RL_ASSERT(0 <= isr_counter);
        if (isr_counter < 2)
        {
            MU_EnableInterrupts(MU3_MUB, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
        }
        isr_counter++;

        env_unlock_mutex(platform_lock);
        return 0;
    }
    else
    {
        return -1;
    }
}

int32_t platform_deinit_interrupt(uint32_t vector_id)
{
    if (platform_lock != ((void *)0))
    {
        env_lock_mutex(platform_lock);

        RL_ASSERT(0 < isr_counter);
        isr_counter--;
        if (isr_counter < 2)
        {
            MU_DisableInterrupts(MU3_MUB, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
        }

        /* Unregister ISR from environment layer */
        env_unregister_isr(vector_id);

        env_unlock_mutex(platform_lock);

        return 0;
    }
    else
    {
        return -1;
    }
}

void platform_notify(uint32_t vector_id)
{
    env_lock_mutex(platform_lock);
#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
    (void)MCMGR_TriggerEventForce(kMCMGR_Core1, kMCMGR_RemoteRPMsgEvent, (uint16_t)(vector_id & 0xFFFF));
#else
    (void)MU_TriggerInterrupts(MU3_MUB, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
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

    /* Calculate the CPU loops to delay, each loop has approx. 6 cycles */
    loop = SystemCoreClock / 6U / 1000U * num_msec;

    while (loop > 0U)
    {
        asm("nop");
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
    return (xthal_get_interrupt() & xthal_get_intenable());
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

#ifdef SDK_OS_BAREMETAL
    _xtos_interrupt_enable(6);
#else
    xos_interrupt_enable(6);
#endif
    disable_counter--;
    return 0;
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

#ifdef SDK_OS_BAREMETAL
    _xtos_interrupt_disable(6);
#else
    xos_interrupt_disable(6);
#endif
    disable_counter++;
    return 0;
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
 * platform_cache_flush
 *
 * Empty implementation
 *
 */
void platform_cache_flush(void *data, uint32_t len)
{
}

/**
 * platform_cache_invalidate
 *
 * Empty implementation
 *
 */
void platform_cache_invalidate(void *data, uint32_t len)
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

    #if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
    mcmgr_status_t retval = kStatus_MCMGR_Error;
    retval                = MCMGR_RegisterEvent(kMCMGR_RemoteRPMsgEvent, mcmgr_event_handler, ((void *)0));
    if (kStatus_MCMGR_Success != retval)
    {
        return -1;
    }
    #else
    MU_Init(MU3_MUB);
    /* Register interrupt handler for MU_B on HiFi1 */
#ifdef SDK_OS_BAREMETAL
    _xtos_set_interrupt_handler(6, MU3_B_IRQHandler);
#else
    xos_register_interrupt_handler(6, MU3_B_IRQHandler, ((void *)0));
#endif
#endif

    return 0;
}

/**
 * platform_deinit
 *
 * platform/environment deinit process
 */
int32_t platform_deinit(void)
{
    MU_Deinit(MU3_MUB);
#ifdef SDK_OS_BAREMETAL
    _xtos_set_interrupt_handler(6, ((void *)0));
#else
    xos_register_interrupt_handler(6, ((void *)0), ((void *)0));
#endif

    /* Delete lock used in multi-instanced RPMsg */
    env_delete_mutex(platform_lock);
    platform_lock = ((void *)0);
    return 0;
}
