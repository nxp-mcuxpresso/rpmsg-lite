/*
 * Copyright 2021-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "rpmsg_default_config.h"
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

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

#define APP_FUSION_M33_MU MU1_MUB

static int32_t isr_counter     = 0;
static int32_t disable_counter = 0;
static void *platform_lock;
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
static LOCK_STATIC_CONTEXT platform_lock_static_ctxt;
#endif

void MU1_B_IRQHandler(void *arg)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(APP_FUSION_M33_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(APP_FUSION_M33_MU, RPMSG_MU_CHANNEL); /* Read message from RX register. */
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_COM_ID << 3)));
    }
}

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
            MU_EnableInterrupts(APP_FUSION_M33_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
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
        /* Prepare the MU Hardware */
        env_lock_mutex(platform_lock);

        RL_ASSERT(0 < isr_counter);
        isr_counter--;

        if (isr_counter < 2)
        {
            MU_DisableInterrupts(APP_FUSION_M33_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
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
    MU_SendMsg(APP_FUSION_M33_MU, RPMSG_MU_CHANNEL, msg);
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
    _xtos_interrupt_enable(MU1_B_IRQn);
#else
    xos_interrupt_enable(MU1_B_IRQn);
#endif
    disable_counter--;
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

#ifdef SDK_OS_BAREMETAL
    _xtos_interrupt_disable(MU1_B_IRQn);
#else
    xos_interrupt_disable(MU1_B_IRQn);
#endif
    disable_counter++;
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
    uintptr_t tmp = (uintptr_t)(char *)addr;
    if (tmp < 0x880000u)
    {
        return ((tmp | 0x20000000u) - 800000u);
    }
    else if (tmp < 0x8C0000u)
    {
        return (tmp - 0x800000u + 0x0FFC0000u);
    }
    else
    {
        return tmp;
    }
}

/**
 * platform_patova
 *
 * Dummy implementation
 *
 */
void *platform_patova(uintptr_t addr)
{
    addr &= 0xEFFFFFFFu;
    if (addr < 0x10000000u) /* SSRAM P7. */
    {
        return ((void *)(char *)(addr - 0x0FFC0000u + 0x880000u));
    }
    else if (addr < 0x20080000u) /* SSRAM P0-P6 */
    {
        return ((void *)(char *)((addr & 0x0FFFFFFFu) + 0x800000u));
    }
    else
    {
        return (void *)(char *)addr;
    }
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
 */
int32_t platform_get_custom_shmem_config(uint32_t link_id, rpmsg_platform_shmem_config_t *cfg)
{
    /* Only MU instance. */
    cfg->buffer_payload_size = RL_BUFFER_PAYLOAD_SIZE(link_id);
    cfg->buffer_count        = RL_BUFFER_COUNT(link_id);

    switch (link_id)
    {
        case RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_SRTM_LINK_ID:
        case RL_PLATFORM_IMX8ULP_M33_FUSION_DSP_USER_LINK_ID:
            cfg->vring_size  = RL_VRING_SIZE_M33_FUSION_DSP_COM;
            cfg->vring_align = RL_VRING_ALIGN_M33_FUSION_DSP_COM;
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
    MU_Init(APP_FUSION_M33_MU);
    /* Create lock used in multi-instanced RPMsg */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
    if (0 != env_create_mutex(&platform_lock, 1, &platform_lock_static_ctxt))
#else
    if (0 != env_create_mutex(&platform_lock, 1))
#endif
    {
        return -1;
    }

    /* Register interrupt handler for MU_B on HiFi4 */
#ifdef SDK_OS_BAREMETAL
    _xtos_set_interrupt_handler(MU1_B_IRQn, MU1_B_IRQHandler);
#else
    xos_register_interrupt_handler(MU1_B_IRQn, MU1_B_IRQHandler, ((void *)0));
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
    MU_Deinit(APP_FUSION_M33_MU);
#ifdef SDK_OS_BAREMETAL
    _xtos_set_interrupt_handler(MU1_B_IRQn, ((void *)0));
#else
    xos_register_interrupt_handler(MU1_B_IRQn, ((void *)0), ((void *)0));
#endif

    /* Delete lock used in multi-instanced RPMsg */
    env_delete_mutex(platform_lock);
    platform_lock = ((void *)0);
    return 0;
}
