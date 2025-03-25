/*
 * Copyright 2024-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>

#include "rpmsg_platform.h"
#include "rpmsg_env.h"

#include "fsl_device_registers.h"
#include "fsl_mu.h"
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
#include "fsl_cache.h"
#include "fsl_ezhv.h"
#endif

#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
#include "mcmgr.h"
#endif

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

#define APP_M33_0_M33_1_MU      MU1_MUA
#define APP_M33_0_M33_1_MU_IRQn MU1_A_IRQn
#define APP_M33_1_M33_0_MU      MU1_MUB
#define APP_M33_1_M33_0_MU_IRQn MU1_B_IRQn
#define APP_M33_0_HIFI4_MU      MU4_MUA
#define APP_M33_0_HIFI4_MU_IRQn MU4_A_IRQn
#define APP_M33_1_HIFI1_MU      MU3_MUA
#define APP_M33_1_HIFI1_MU_IRQn MU3_A_IRQn
#define APP_M33_0_EZHV_IRQn     EZHV_IRQn

#define APP_MU_IRQ_PRIORITY (3U)
static int32_t isr_counter0 = 0; /* RL_PLATFORM_IMXRT700_M33_0_M33_1_LINK_ID isr counter */
static int32_t isr_counter1 = 0; /* RL_PLATFORM_IMXRT700_M33_0_HIFI4_LINK_ID isr counter */
static int32_t isr_counter2 = 0; /* RL_PLATFORM_IMXRT700_M33_1_HIFI1_LINK_ID isr counter */
static int32_t isr_counter3 = 0; /* RL_PLATFORM_IMXRT700_M33_0_EZHV_LINK_ID isr counter */

static int32_t disable_counter0 = 0;
static int32_t disable_counter1 = 0;
static int32_t disable_counter2 = 0;
static int32_t disable_counter3 = 0;
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
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
void MU1_A_IRQHandler(void)
{
    uint32_t flags;
    flags = MU_GetStatusFlags(APP_M33_0_M33_1_MU);
    if (((uint32_t)kMU_GenInt0Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(APP_M33_0_M33_1_MU, (uint32_t)kMU_GenInt0Flag);
        env_isr(RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID << 3U);
    }
    if (((uint32_t)kMU_GenInt1Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(APP_M33_0_M33_1_MU, (uint32_t)kMU_GenInt1Flag);
        env_isr((uint32_t)(0x01U | (RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID << 3U)));
    }
}
void MU4_A_IRQHandler(void)
{
    uint32_t flags;
    flags = MU_GetStatusFlags(APP_M33_0_HIFI4_MU);
    if (((uint32_t)kMU_GenInt0Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(APP_M33_0_HIFI4_MU, (uint32_t)kMU_GenInt0Flag);
        env_isr(RL_PLATFORM_IMXRT700_M33_0_HIFI4_COM_ID << 3U);
    }
    if (((uint32_t)kMU_GenInt1Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(APP_M33_0_HIFI4_MU, (uint32_t)kMU_GenInt1Flag);
        env_isr((uint32_t)(0x01U | (RL_PLATFORM_IMXRT700_M33_0_HIFI4_COM_ID << 3U)));
    }
}
static void ezhv_callback0(void *param)
{
    env_isr(RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID << 3U);
}
static void ezhv_callback1(void *param)
{
    env_isr((uint32_t)(0x01U | (RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID << 3U)));
}
#elif (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
       defined(MIMXRT798S_cm33_core1_SERIES))
void MU1_B_IRQHandler(void)
{
    uint32_t flags;
    flags = MU_GetStatusFlags(APP_M33_1_M33_0_MU);
    if (((uint32_t)kMU_GenInt0Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(APP_M33_1_M33_0_MU, (uint32_t)kMU_GenInt0Flag);
        env_isr(RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID << 3U);
    }
    if (((uint32_t)kMU_GenInt1Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(APP_M33_1_M33_0_MU, (uint32_t)kMU_GenInt1Flag);
        env_isr((uint32_t)(0x01U | (RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID << 3U)));
    }
}
void MU3_A_IRQHandler(void)
{
    uint32_t flags;
    flags = MU_GetStatusFlags(APP_M33_1_HIFI1_MU);
    if (((uint32_t)kMU_GenInt0Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(APP_M33_1_HIFI1_MU, (uint32_t)kMU_GenInt0Flag);
        env_isr(RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID << 3U);
    }
    if (((uint32_t)kMU_GenInt1Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(APP_M33_1_HIFI1_MU, (uint32_t)kMU_GenInt1Flag);
        env_isr((uint32_t)(0x01U | (RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID << 3U)));
    }
}
#endif
#endif

static void platform_global_isr_disable(void)
{
    __asm volatile("cpsid i");
}

static void platform_global_isr_enable(void)
{
    __asm volatile("cpsie i");
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
            case RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID:
                RL_ASSERT(0 <= isr_counter0);
                if (isr_counter0 < 2)
                {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                    MU_EnableInterrupts(APP_M33_0_M33_1_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#elif (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
       defined(MIMXRT798S_cm33_core1_SERIES))
                    MU_EnableInterrupts(APP_M33_1_M33_0_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
                }
                isr_counter0++;
                break;
            case RL_PLATFORM_IMXRT700_M33_0_HIFI4_COM_ID:
                RL_ASSERT(0 <= isr_counter1);
                if (isr_counter1 < 2)
                {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                    MU_EnableInterrupts(APP_M33_0_HIFI4_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
                }
                isr_counter1++;
                break;
            case RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID:
                RL_ASSERT(0 <= isr_counter2);
                if (isr_counter2 < 2)
                {
#if (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
     defined(MIMXRT798S_cm33_core1_SERIES))
                    MU_EnableInterrupts(APP_M33_1_HIFI1_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
                }
                isr_counter2++;
                break;
            case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
                RL_ASSERT(0 <= isr_counter3);
                if (isr_counter3 < 2)
                {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                    EZHV_EnableEzhv2ArmIntChan(kEZHV_EzhvToArmIntChan0);
                    EZHV_EnableEzhv2ArmIntChan(kEZHV_EzhvToArmIntChan1);
#endif
                }
                isr_counter3++;
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
            case RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID:
                RL_ASSERT(0 < isr_counter0);
                isr_counter0--;
                if (isr_counter0 < 2)
                {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                    MU_DisableInterrupts(APP_M33_0_M33_1_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#elif (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
       defined(MIMXRT798S_cm33_core1_SERIES))
                    MU_DisableInterrupts(APP_M33_1_M33_0_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
                }
                break;
            case RL_PLATFORM_IMXRT700_M33_0_HIFI4_COM_ID:
                RL_ASSERT(0 < isr_counter1);
                isr_counter1--;
                if (isr_counter1 < 2)
                {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                    MU_DisableInterrupts(APP_M33_0_HIFI4_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
                }
                break;
            case RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID:
                RL_ASSERT(0 < isr_counter2);
                isr_counter2--;
                if (isr_counter2 < 2)
                {
#if (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
     defined(MIMXRT798S_cm33_core1_SERIES))
                    MU_DisableInterrupts(APP_M33_1_HIFI1_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
                }
                break;
            case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
                RL_ASSERT(0 <= isr_counter3);
                if (isr_counter3 < 2)
                {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                    EZHV_DisableEzhv2ArmIntChan(kEZHV_EzhvToArmIntChan0);
                    EZHV_DisableEzhv2ArmIntChan(kEZHV_EzhvToArmIntChan1);
#endif
                }
                isr_counter3++;
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
        case RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID:
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
            (void)MU_TriggerInterrupts(APP_M33_0_M33_1_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#elif (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
       defined(MIMXRT798S_cm33_core1_SERIES))
            (void)MU_TriggerInterrupts(APP_M33_1_M33_0_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
            break;
        case RL_PLATFORM_IMXRT700_M33_0_HIFI4_COM_ID:
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
            (void)MU_TriggerInterrupts(APP_M33_0_HIFI4_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#elif (defined(MIMXRT735S_hifi4_SERIES) || defined(MIMXRT758S_hifi4_SERIES) || defined(MIMXRT798S_hifi4_SERIES))
            //(void)MU_TriggerInterrupts(APP_M33_1_M33_0_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
            break;
        case RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID:
#if (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
     defined(MIMXRT798S_cm33_core1_SERIES))
            (void)MU_TriggerInterrupts(APP_M33_1_HIFI1_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#elif (defined(MIMXRT735S_hifi1_SERIES) || defined(MIMXRT758S_hifi1_SERIES) || defined(MIMXRT798S_hifi1_SERIES))
            //(void)MU_TriggerInterrupts(APP_M33_1_M33_0_MU, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
            break;
        case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
            if(RL_GET_Q_ID(vector_id))
            {
                EZHV_EnableArm2EzhvInt(kEZHV_ARM2EZHV_MEI);
            }
            else
            {
                EZHV_EnableArm2EzhvInt(kEZHV_ARM2EZHV_MSI);
            }
#endif
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
    return (((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0UL) ? 1 : 0);
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
    platform_global_isr_disable();
    switch (RL_GET_COM_ID(vector_id))
    {
        case RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID:
            RL_ASSERT(0 < disable_counter0);
            disable_counter0--;
            if (disable_counter0 == 0)
            {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                NVIC_EnableIRQ(APP_M33_0_M33_1_MU_IRQn);
#elif (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
       defined(MIMXRT798S_cm33_core1_SERIES))
                NVIC_EnableIRQ(APP_M33_1_M33_0_MU_IRQn);
#endif
            }
            break;
        case RL_PLATFORM_IMXRT700_M33_0_HIFI4_COM_ID:
            RL_ASSERT(0 < disable_counter1);
            disable_counter1--;
            if (disable_counter1 == 0)
            {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                NVIC_EnableIRQ(APP_M33_0_HIFI4_MU_IRQn);
#endif
            }
            break;
        case RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID:
            RL_ASSERT(0 < disable_counter2);
            disable_counter2--;
            if (disable_counter2 == 0)
            {
#if (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
     defined(MIMXRT798S_cm33_core1_SERIES))
                NVIC_EnableIRQ(APP_M33_1_HIFI1_MU_IRQn);
#endif
            }
            break;
        case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
            RL_ASSERT(0 < disable_counter3);
            disable_counter3--;
            if (disable_counter3 == 0)
            {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                NVIC_EnableIRQ(APP_M33_0_EZHV_IRQn);
#endif
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
        case RL_PLATFORM_IMXRT700_M33_0_M33_1_COM_ID:
            RL_ASSERT(0 <= disable_counter0);
            if (disable_counter0 == 0)
            {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                NVIC_DisableIRQ(APP_M33_0_M33_1_MU_IRQn);
#elif (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
       defined(MIMXRT798S_cm33_core1_SERIES))
                NVIC_DisableIRQ(APP_M33_1_M33_0_MU_IRQn);
#endif
            }
            disable_counter0++;
            break;
        case RL_PLATFORM_IMXRT700_M33_0_HIFI4_COM_ID:
            RL_ASSERT(0 <= disable_counter1);
            if (disable_counter1 == 0)
            {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                NVIC_DisableIRQ(APP_M33_0_HIFI4_MU_IRQn);
#endif
            }
            disable_counter1++;
            break;
        case RL_PLATFORM_IMXRT700_M33_1_HIFI1_COM_ID:
            RL_ASSERT(0 <= disable_counter2);
            if (disable_counter2 == 0)
            {
#if (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
     defined(MIMXRT798S_cm33_core1_SERIES))
                NVIC_DisableIRQ(APP_M33_1_HIFI1_MU_IRQn);
#endif
            }
            disable_counter2++;
            break;
        case RL_PLATFORM_IMXRT700_M33_0_EZHV_COM_ID:
            RL_ASSERT(0 <= disable_counter3);
            if (disable_counter3 == 0)
            {
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
                NVIC_DisableIRQ(APP_M33_0_EZHV_IRQn);
#endif
            }
            disable_counter3++;
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

    /* Cache is handled only on core0 */
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
    XCACHE_CleanCacheByRange((uint32_t)(char *)data, len);
#endif
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

    /* Cache is handled only on core0 */
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
    XCACHE_InvalidateCacheByRange((uint32_t)(char *)data, len);
#endif
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
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
    MU_Init(APP_M33_0_M33_1_MU);
    NVIC_SetPriority(APP_M33_0_M33_1_MU_IRQn, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(APP_M33_0_M33_1_MU_IRQn);

    MU_Init(APP_M33_0_HIFI4_MU);
    NVIC_SetPriority(APP_M33_0_HIFI4_MU_IRQn, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(APP_M33_0_HIFI4_MU_IRQn);

    EZHV_SetCallback(ezhv_callback0, 0, NULL);
    EZHV_SetCallback(ezhv_callback1, 1, NULL);
    NVIC_SetPriority(APP_M33_0_EZHV_IRQn, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(APP_M33_0_EZHV_IRQn);
#elif (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
       defined(MIMXRT798S_cm33_core1_SERIES))
    MU_Init(APP_M33_1_M33_0_MU);
    NVIC_SetPriority(APP_M33_1_M33_0_MU_IRQn, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(APP_M33_1_M33_0_MU_IRQn);

    MU_Init(APP_M33_1_HIFI1_MU);
    NVIC_SetPriority(APP_M33_1_HIFI1_MU_IRQn, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(APP_M33_1_HIFI1_MU_IRQn);
#endif
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
#if (defined(MIMXRT735S_cm33_core0_SERIES) || defined(MIMXRT758S_cm33_core0_SERIES) || \
     defined(MIMXRT798S_cm33_core0_SERIES))
    MU_Deinit(APP_M33_0_M33_1_MU);
    MU_Deinit(APP_M33_0_HIFI4_MU);
#elif (defined(MIMXRT735S_cm33_core1_SERIES) || defined(MIMXRT758S_cm33_core1_SERIES) || \
       defined(MIMXRT798S_cm33_core1_SERIES))
    MU_Deinit(APP_M33_1_M33_0_MU);
    MU_Deinit(APP_M33_1_HIFI1_MU);
#endif

    /* Delete lock used in multi-instanced RPMsg */
    env_delete_mutex(platform_lock);
    platform_lock = ((void *)0);
    return 0;
}
