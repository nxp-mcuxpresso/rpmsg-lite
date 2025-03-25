/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>

#include "rpmsg_platform.h"
#include "rpmsg_env.h"

#include "fsl_device_registers.h"

#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
#include "mcmgr.h"
#endif

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

static int32_t isr_counter0 = 0; /* RL_PLATFORM_IMXRT700_M33_0_EZHV_LINK_ID isr counter */

static int32_t disable_counter0 = 0;
static void *platform_lock;
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
static LOCK_STATIC_CONTEXT platform_lock_static_ctxt;
#endif

#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
static void mcmgr_event_handler(uint16_t vring_idx, void *context)
{
    env_isr((uint32_t)vring_idx);
}

#else
void MachineSoftInt_Handler(void)
{
    env_isr(RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID << 3U);
}
void MachineExtInt_Handler(void)
{
    env_isr((uint32_t)(0x01U | (RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID << 3U)));
}
#endif

static void platform_global_isr_disable(void)
{
    DisableGlobalIRQ();
}

static void platform_global_isr_enable(void)
{
    EnableGlobalIRQ(CSR_MSTATUS_MIE);
}

int32_t platform_init_interrupt(uint32_t vector_id, void *isr_data)
{
    if (platform_lock != ((void *)0))
    {
        /* Register ISR to environment layer */
        env_register_isr(vector_id, isr_data);

        /* Prepare the MU Hardware, enable channel 1 interrupt */
        env_lock_mutex(platform_lock);

        switch (RL_GET_COM_ID(vector_id))
        {
            case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
                RL_ASSERT(0 <= isr_counter0);
                if (isr_counter0 < 2)
                {
                    EnableMachineModeInt(kEZHV_Mie_Msie);
                    EnableMachineModeInt(kEZHV_Mie_Meie);
                }
                isr_counter0++;
                break;
            default:
                /* All the cases have been listed above, the default clause should not be reached. */
                break;
        }

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
        /* Prepare the MU Hardware */
        env_lock_mutex(platform_lock);

        switch (RL_GET_COM_ID(vector_id))
        {
            case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
                RL_ASSERT(0 <= isr_counter0);
                if (isr_counter0 < 2)
                {
                    DisableMachineModeInt(kEZHV_Mie_Msie);
                    DisableMachineModeInt(kEZHV_Mie_Meie);
                }
                isr_counter0++;
                break;
            default:
                /* All the cases have been listed above, the default clause should not be reached. */
                break;
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
    (void)MCMGR_TriggerEventForce(kMCMGR_RemoteRPMsgEvent, (uint16_t)RL_GET_Q_ID(vector_id));
#else
    switch (RL_GET_COM_ID(vector_id))
    {
        case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
            XZMSG_IndIntChan(1 << RL_GET_Q_ID(vector_id));
            break;
        default:
            /* All the cases have been listed above, the default clause should not be reached. */
            break;
    }
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

    /* Calculate the CPU loops to delay, each loop has 3 cycles */
    loop = SystemCoreClock / 3U / 1000U * num_msec;

    /* There's some difference among toolchains, 3 or 4 cycles each loop */
    while (loop > 0U)
    {
        __NOP();
        loop--;
    }
}

#if (!defined(CPU_MIMXRT798SGAWAR_ezhv) && !defined(CPU_MIMXRT798SGFOA_ezhv))
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
    return (((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0UL) ? 1 : 0);
}
#endif

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
    platform_global_isr_disable();
    switch (RL_GET_COM_ID(vector_id))
    {
        case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
            RL_ASSERT(0 < disable_counter0);
            disable_counter0--;
            if (disable_counter0 == 0)
            {
                EnableGlobalIRQ(CSR_MSTATUS_MIE);
            }
            break;
        default:
            /* All the cases have been listed above, the default clause should not be reached. */
            break;
    }
    platform_global_isr_enable();
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
    platform_global_isr_disable();
    /* virtqueues use the same NVIC vector
       if counter is set - the interrupts are disabled */
    switch (RL_GET_COM_ID(vector_id))
    {
        case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
            RL_ASSERT(0 <= disable_counter0);
            if (disable_counter0 == 0)
            {
                DisableGlobalIRQ();
            }
            disable_counter0++;
            break;
        default:
            /* All the cases have been listed above, the default clause should not be reached. */
            break;
    }
    platform_global_isr_enable();
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
 * Invalidates Cache.
 *
 * @param data Pointer to start of memory to flush
 * @param len Length of memory to flush
 *
 */
void platform_cache_flush(void *data, uint32_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return;
    }
}

/**
 * platform_cache_invalidate
 *
 * Invalidates Cache.
 *
 * @param data Pointer to start of memory to invalidate
 * @param len Length of memory to invalidate
 *
 */
void platform_cache_invalidate(void *data, uint32_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return;
    }
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
#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
    mcmgr_status_t retval = kStatus_MCMGR_Error;
    retval                = MCMGR_RegisterEvent(kMCMGR_RemoteRPMsgEvent, mcmgr_event_handler, ((void *)0));
    if (kStatus_MCMGR_Success != retval)
    {
        return -1;
    }
#else
    EnableGlobalIRQ(CSR_MSTATUS_MIE);
#endif

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
