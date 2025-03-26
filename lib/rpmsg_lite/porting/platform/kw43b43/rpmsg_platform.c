/*
 * Copyright 2024-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>

#include "rpmsg_platform.h"
#include "rpmsg_default_config.h"
#include "rpmsg_lite.h"

#include "rpmsg_env.h"

#include "fsl_device_registers.h"
#include "fsl_mu.h"

#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
#include "mcmgr.h"
#endif

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

#define RL_PLATFORM_SHMEM_CFG_IDENTIFIER_LENGTH (12U)

/* Generator for CRC calculations. */
#define POLGEN 0x1021U

static int32_t isr_counter     = 0;
static int32_t disable_counter = 0;
static void *platform_lock;
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
static LOCK_STATIC_CONTEXT platform_lock_static_ctxt;
#endif

/* This structure is used to validate that shmem config that is stored in SMU2 is valid */
typedef struct rpmsg_platform_shmem_config_protected
{
    uint8_t identificationWord[RL_PLATFORM_SHMEM_CFG_IDENTIFIER_LENGTH];
    rpmsg_platform_shmem_config_t config;
    uint16_t shmemConfigCrc;
} rpmsg_platform_shmem_config_protected_t;

static const uint8_t ShmemConfigIdentifier[RL_PLATFORM_SHMEM_CFG_IDENTIFIER_LENGTH] = {"SMEM_CONFIG:"};

/* Compute CRC to protect shared memory strcuture stored in RAM by application core and retrieve by cm33_core1 */
static uint16_t platform_compute_crc_over_shmem_struct(rpmsg_platform_shmem_config_protected_t *protected_structure);

static uint32_t first_time                        = RL_TRUE;
static rpmsg_platform_shmem_config_t shmem_config = {0U};

#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
static void mcmgr_event_handler(uint16_t vring_idx, void *context)
{
    env_isr((uint32_t)vring_idx);
}
#else
static void mu_isr(MU_Type *base)
{
    uint32_t flags;
    flags = MU_GetStatusFlags(base);
    if (((uint32_t)kMU_GenInt0Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(base, (uint32_t)kMU_GenInt0Flag);
        env_isr(0);
    }
    if (((uint32_t)kMU_GenInt1Flag & flags) != 0UL)
    {
        MU_ClearStatusFlags(base, (uint32_t)kMU_GenInt1Flag);
        env_isr(1);
    }
}

int32_t MU0_IRQHandler(void)
{
#if defined(FSL_FEATURE_MU_SIDE_A)
    mu_isr(MU0_MUA);
#elif defined(FSL_FEATURE_MU_SIDE_B)
    mu_isr(MU0_MUB);
#endif
    return 0;
}
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

        env_lock_mutex(platform_lock);

        RL_ASSERT(0 <= isr_counter);
        if (isr_counter < 2)
        {
#if defined(FSL_FEATURE_MU_SIDE_A)
            MU_EnableInterrupts(MU0_MUA, MU_GI_INTR(1UL << vector_id));
#elif defined(FSL_FEATURE_MU_SIDE_B)
            MU_EnableInterrupts(MU0_MUB, MU_GI_INTR(1UL << vector_id));
#endif
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
#if defined(FSL_FEATURE_MU_SIDE_A)
            MU_DisableInterrupts(MU0_MUA, MU_GI_INTR(1UL << vector_id));
#elif defined(FSL_FEATURE_MU_SIDE_B)
            MU_DisableInterrupts(MU0_MUB, MU_GI_INTR(1UL << vector_id));
#endif
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
/* Write directly into the MU Control Register to trigger General Purpose Interrupt Request (GIR).
   No need to wait until the previous interrupt is processed because the same value
   of the virtqueue ID is used for GIR mask when triggering the ISR for the receiver side.
   The whole queue of received buffers for associated virtqueue is then handled in the ISR
   on the receiver side. */
#if defined(FSL_FEATURE_MU_SIDE_A)
    (void)MU_TriggerInterrupts(MU0_MUA, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#elif defined(FSL_FEATURE_MU_SIDE_B)
    (void)MU_TriggerInterrupts(MU0_MUB, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
#endif
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
    RL_ASSERT(0 < disable_counter);

    platform_global_isr_disable();
    disable_counter--;

    if (disable_counter == 0)
    {
#if defined(FSL_FEATURE_MU_SIDE_A)
        NVIC_EnableIRQ(MU0_IRQn);
#elif defined(FSL_FEATURE_MU_SIDE_B)
        NVIC_EnableIRQ(MU0_IRQn);
#endif
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
    RL_ASSERT(0 <= disable_counter);

    platform_global_isr_disable();
    /* virtqueues use the same NVIC vector
       if counter is set - the interrupts are disabled */
    if (disable_counter == 0)
    {
#if defined(FSL_FEATURE_MU_SIDE_A)
        NVIC_DisableIRQ(MU0_IRQn);
        NVIC_SetPriority(MU0_IRQn, 2);
#elif defined(FSL_FEATURE_MU_SIDE_B)
        NVIC_DisableIRQ(MU0_IRQn);
#endif
    }

    disable_counter++;
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
    /* The MU peripheral driver is not initialized here because it covers also
    the secondary core booting controls and it needs to be initialized earlier
    in the application code */

#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
    mcmgr_status_t retval = kStatus_MCMGR_Error;
    retval                = MCMGR_RegisterEvent(kMCMGR_RemoteRPMsgEvent, mcmgr_event_handler, ((void *)0));
    if (kStatus_MCMGR_Success != retval)
    {
        return -1;
    }
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

void platform_set_static_shmem_config(void)
{
    extern uint32_t rpmsg_sh_mem_start[];
    rpmsg_platform_shmem_config_protected_t protec_shmem_struct;

    /* Identifier at the beginning of the structure that will be used to verify on cm33_core1 side validity of the structure */
    memcpy(&(protec_shmem_struct.identificationWord), ShmemConfigIdentifier, RL_PLATFORM_SHMEM_CFG_IDENTIFIER_LENGTH);

    /* Fill shared memory structure with setting from the app core */
    protec_shmem_struct.config.buffer_payload_size = RL_BUFFER_PAYLOAD_SIZE;
    protec_shmem_struct.config.buffer_count        = RL_BUFFER_COUNT;
    protec_shmem_struct.config.vring_size          = VRING_SIZE;
    protec_shmem_struct.config.vring_align         = VRING_ALIGN;

    /* Calculate and set CRC of the strucuture */
    protec_shmem_struct.shmemConfigCrc = platform_compute_crc_over_shmem_struct(&protec_shmem_struct);

    /* Store in SMU2 the all structure */
    memcpy(rpmsg_sh_mem_start, &protec_shmem_struct, sizeof(rpmsg_platform_shmem_config_protected_t));
}

uint32_t platform_get_custom_shmem_config(uint32_t link_id, rpmsg_platform_shmem_config_t *config)
{
    extern uint32_t rpmsg_sh_mem_start[];
    rpmsg_platform_shmem_config_protected_t protec_shmem_struct = {0U};

    do
    {
        if (first_time == RL_FALSE)
        {
            /* Variable shmem_config is already set if this is not the fisrt call */
            break;
        }

        first_time = RL_FALSE;

        /* Copy the full structure in local variable */
        memcpy(&protec_shmem_struct, rpmsg_sh_mem_start, sizeof(rpmsg_platform_shmem_config_protected_t));

        /* By default set the values of the MR3 connectivity release */
        shmem_config.buffer_payload_size = 496U;
        shmem_config.buffer_count        = 4U;
        shmem_config.vring_size          = 0x80U;
        shmem_config.vring_align         = 0x10U;

        if (memcmp(&(protec_shmem_struct.identificationWord), ShmemConfigIdentifier, RL_PLATFORM_SHMEM_CFG_IDENTIFIER_LENGTH) !=
            0)
        {
            break;
        }
        if (platform_compute_crc_over_shmem_struct(&protec_shmem_struct) != protec_shmem_struct.shmemConfigCrc)
        {
            break;
        }
        /* If the identifier and the CRC are correct we can copy the shared memory config stored in SMU2 in local
         * variable */
        memcpy(&shmem_config, &(protec_shmem_struct.config), sizeof(rpmsg_platform_shmem_config_t));

    } while (false);

    memcpy(config, &shmem_config, sizeof(rpmsg_platform_shmem_config_t));

    (void)link_id;

    return 0U;
}

static uint16_t platform_compute_crc_over_shmem_struct(rpmsg_platform_shmem_config_protected_t *protec_shmem_struct)
{
    uint16_t computedCRC = 0U;
    uint8_t crcA;
    uint8_t byte = 0U;

    uint8_t *ptr = (uint8_t *)(&protec_shmem_struct->config);
    uint32_t len = ((uint32_t)(uint8_t *)(&protec_shmem_struct->shmemConfigCrc) -
                    (uint32_t)(uint8_t *)(&protec_shmem_struct->config));
    while (len != 0U)
    {
        byte = *ptr;
        computedCRC ^= ((uint16_t)byte << 8U);
        for (crcA = 8U; crcA != 0U; crcA--)
        {
            if ((computedCRC & 0x8000U) != 0U)
            {
                computedCRC <<= 1U;
                computedCRC ^= POLGEN;
            }
            else
            {
                computedCRC <<= 1U;
            }
        }
        --len;
        ++ptr;
    }
    return computedCRC;
}
