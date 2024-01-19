/*
 * Copyright 2021-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>

#include "rpmsg_default_config.h"
#include "rpmsg_platform.h"
#include "rpmsg_env.h"

#include "fsl_device_registers.h"
#include "fsl_mu.h"

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

#define APP_M33_A35_MU             MU0_MUA
#define APP_M33_A35_MU_IRQn        MU0_A_IRQn
#define APP_M33_FUSION_DSP_MU      MU1_MUA
#define APP_M33_FUSION_DSP_MU_IRQn MU1_A_IRQn
#define APP_M33_HIFI4_MU           MU2_MUA
#define APP_M33_HIFI4_MU_IRQn      MU2_A_IRQn

#define APP_MU_IRQ_PRIORITY         (3U)
#define APP_MU_A35_SIDE_READY       (0x1U)
#define APP_MU_A35_WAIT_INTERVAL_MS (10U)

static int32_t isr_counter0 = 0; /* RL_PLATFORM_IMX8ULP_M33_A35_USER_LINK_ID isr counter */
static int32_t isr_counter1 = 0; /* RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_USER_LINK_ID isr counter */
static int32_t isr_counter2 = 0; /* RL_PLATFORM_IMX8ULP_M33_HIFI4_USER_LINK_ID isr counter */

static int32_t disable_counter0 = 0;
static int32_t disable_counter1 = 0;
static int32_t disable_counter2 = 0;
static void *platform_lock;
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
static LOCK_STATIC_CONTEXT platform_lock_static_ctxt;
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
            case RL_PLATFORM_IMX8ULP_M33_A35_COM_ID:
                RL_ASSERT(0 <= isr_counter0);
                if (isr_counter0 == 0)
                {
                    MU_EnableInterrupts(APP_M33_A35_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
                }
                isr_counter0++;
                break;
            case RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_COM_ID:
                RL_ASSERT(0 <= isr_counter1);
                if (isr_counter1 == 0)
                {
                    MU_EnableInterrupts(APP_M33_FUSION_DSP_MU,
                                        (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
                }
                isr_counter1++;
                break;
            case RL_PLATFORM_IMX8ULP_M33_HIFI4_COM_ID:
                RL_ASSERT(0 <= isr_counter2);
                if (isr_counter2 == 0)
                {
                    MU_EnableInterrupts(APP_M33_HIFI4_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
                }
                isr_counter2++;
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
            case RL_PLATFORM_IMX8ULP_M33_A35_COM_ID:
                RL_ASSERT(0 < isr_counter0);
                isr_counter0--;
                if (isr_counter0 == 0)
                {
                    MU_DisableInterrupts(APP_M33_A35_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
                }
                break;
            case RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_COM_ID:
                RL_ASSERT(0 < isr_counter1);
                isr_counter1--;
                if (isr_counter1 == 0)
                {
                    MU_DisableInterrupts(APP_M33_FUSION_DSP_MU,
                                         (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
                }
                break;
            case RL_PLATFORM_IMX8ULP_M33_HIFI4_COM_ID:
                RL_ASSERT(0 < isr_counter2);
                isr_counter2--;
                if (isr_counter2 == 0)
                {
                    MU_DisableInterrupts(APP_M33_HIFI4_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
                }
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
    /* Only vring id and queue id is needed in msg */
    uint32_t msg = RL_GEN_MU_MSG(vector_id);

    env_lock_mutex(platform_lock);
    /* As Linux suggests, use MU->Data Channel 1 as communication channel */
    switch (RL_GET_COM_ID(vector_id))
    {
        case RL_PLATFORM_IMX8ULP_M33_A35_COM_ID:
            MU_SendMsg(APP_M33_A35_MU, RPMSG_MU_CHANNEL, msg);
            break;
        case RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_COM_ID:
            MU_SendMsg(APP_M33_FUSION_DSP_MU, RPMSG_MU_CHANNEL, msg);
            break;
        case RL_PLATFORM_IMX8ULP_M33_HIFI4_COM_ID:
            MU_SendMsg(APP_M33_HIFI4_MU, RPMSG_MU_CHANNEL, msg);
            break;
        default:
            /* All the cases have been listed above, the default clause should not be reached. */
            break;
    }
    env_unlock_mutex(platform_lock);
}

/*
 * MU Interrrupt RPMsg handler
 */
int32_t MU0_A_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(APP_M33_A35_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(APP_M33_A35_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX8ULP_M33_A35_COM_ID << 3)));
    }

    return 0;
}

int32_t MU1_A_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(APP_M33_FUSION_DSP_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(APP_M33_FUSION_DSP_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_COM_ID << 3)));
    }

    return 0;
}

int32_t MU2_A_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(APP_M33_HIFI4_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(APP_M33_HIFI4_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX8ULP_M33_HIFI4_COM_ID << 3)));
    }

    return 0;
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
        case RL_PLATFORM_IMX8ULP_M33_A35_COM_ID:
            RL_ASSERT(0 < disable_counter0);
            disable_counter0--;
            if (disable_counter0 == 0)
            {
                NVIC_EnableIRQ(APP_M33_A35_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_COM_ID:
            RL_ASSERT(0 < disable_counter1);
            disable_counter1--;
            if (disable_counter1 == 0)
            {
                NVIC_EnableIRQ(APP_M33_FUSION_DSP_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX8ULP_M33_HIFI4_COM_ID:
            RL_ASSERT(0 < disable_counter2);
            disable_counter2--;
            if (disable_counter2 == 0)
            {
                NVIC_EnableIRQ(APP_M33_HIFI4_MU_IRQn);
            }
            break;
        default:
            /* All the cases have been listed above, the default clause should not be reached. */
            break;
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
    platform_global_isr_disable();
    /* virtqueues use the same NVIC vector
       if counter is set - the interrupts are disabled */
    switch (RL_GET_COM_ID(vector_id))
    {
        case RL_PLATFORM_IMX8ULP_M33_A35_COM_ID:
            RL_ASSERT(0 <= disable_counter0);
            disable_counter0++;
            if (disable_counter0 == 0)
            {
                NVIC_DisableIRQ(APP_M33_A35_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_COM_ID:
            RL_ASSERT(0 <= disable_counter1);
            disable_counter1++;
            if (disable_counter1 == 0)
            {
                NVIC_DisableIRQ(APP_M33_FUSION_DSP_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX8ULP_M33_HIFI4_COM_ID:
            RL_ASSERT(0 <= disable_counter2);
            disable_counter2++;
            if (disable_counter2 == 0)
            {
                NVIC_DisableIRQ(APP_M33_HIFI4_MU_IRQn);
            }
            break;
        default:
            /* All the cases have been listed above, the default clause should not be reached. */
            break;
    }
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

#if defined(RL_ALLOW_CUSTOM_SHMEM_CONFIG) && (RL_ALLOW_CUSTOM_SHMEM_CONFIG == 1)
/**
 * platform_get_custom_shmem_config
 *
 * Provide rpmsg_platform_shmem_config structure for the rpmsg_lite instance initialization
 * based on the link_id.
 *
 * @param link_id Link ID provided by the rpmsg_lite init function
 * @param cfg Pointer to the rpmsg_platform_shmem_config structure to be filled
 *
 * @return Status of function execution, 0 on success.
 *
 */
int32_t platform_get_custom_shmem_config(uint32_t link_id, rpmsg_platform_shmem_config_t *cfg)
{
    cfg->buffer_payload_size = RL_BUFFER_PAYLOAD_SIZE(link_id);
    cfg->buffer_count        = RL_BUFFER_COUNT(link_id);

    switch (link_id)
    {
        case RL_PLATFORM_IMX8ULP_M33_A35_SRTM_LINK_ID:
        case RL_PLATFORM_IMX8ULP_M33_A35_USER_LINK_ID:
            cfg->vring_size  = RL_VRING_SIZE_M33_A35_COM;
            cfg->vring_align = RL_VRING_ALIGN_M33_A35_COM;
            break;
        case RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_SRTM_LINK_ID:
        case RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_USER_LINK_ID:
            cfg->vring_size  = RL_VRING_SIZE_M33_FUSION_DSP_COM;
            cfg->vring_align = RL_VRING_ALIGN_M33_FUSION_DSP_COM;
            break;
        case RL_PLATFORM_IMX8ULP_M33_HIFI4_SRTM_LINK_ID:
        case RL_PLATFORM_IMX8ULP_M33_HIFI4_USER_LINK_ID:
            cfg->vring_size  = RL_VRING_SIZE_M33_HIFI4_COM;
            cfg->vring_align = RL_VRING_ALIGN_M33_HIFI4_COM;
            break;
        default:
            /* All the cases have been listed above, the default clause should not be reached. */
            break;
    }
    return 0;
}

#endif /* defined(RL_ALLOW_CUSTOM_SHMEM_CONFIG) && (RL_ALLOW_CUSTOM_SHMEM_CONFIG == 1) */

/**
 * platform_init
 *
 * platform/environment init
 */
int32_t platform_init(void)
{
    /*
     * Prepare for the MU Interrupt
     *  MU must be initialized before rpmsg init is called
     */
    MU_Init(APP_M33_A35_MU);
    NVIC_SetPriority(APP_M33_A35_MU_IRQn, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(APP_M33_A35_MU_IRQn);

    MU_Init(APP_M33_FUSION_DSP_MU);
    NVIC_SetPriority(APP_M33_FUSION_DSP_MU_IRQn, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(APP_M33_FUSION_DSP_MU_IRQn);

    MU_Init(APP_M33_HIFI4_MU);
    NVIC_SetPriority(APP_M33_HIFI4_MU_IRQn, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(APP_M33_HIFI4_MU_IRQn);

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
    MU_Deinit(APP_M33_A35_MU);
    MU_Deinit(APP_M33_FUSION_DSP_MU);
    MU_Deinit(APP_M33_HIFI4_MU);
    /* Delete lock used in multi-instanced RPMsg */
    env_delete_mutex(platform_lock);
    platform_lock = ((void *)0);
    return 0;
}
