/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * MCXE32B dual Cortex-M7 RPMsg-Lite platform porting layer.
 *
 * The build target core is selected at compile time by the device series macro:
 *   MCXE32B_cm7_core0_SERIES -> M7_0 = PRIMARY core   = MU2_A side = kMCMGR_Core0
 *   MCXE32B_cm7_core1_SERIES -> M7_1 = SECONDARY core = MU2_B side = kMCMGR_Core1
 *
 * Both CM7 cores define FSL_FEATURE_MU_SIDE_A and FSL_FEATURE_MU_SIDE_B because
 * each core can access either MU side, so the series macro (not the MU side
 * feature) is used to pick the local MU instance and its dedicated NVIC lines.
 *
 * Inter-core notification uses an MU general purpose interrupt request (GIR).
 * Writing the local MU GCR GIR bit raises the peer general purpose NVIC line
 * (MU2_A_IRQn / MU2_B_IRQn).
 */
#include <stdio.h>
#include <string.h>

#include "rpmsg_platform.h"
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

/*
 * Select the local MU instance and its dedicated NVIC lines based on the
 * device series macro set by the device header.
 */
#if defined(MCXE32B_cm7_core0_SERIES)
/* Building for M7_0 = PRIMARY core, MU2_A side, kMCMGR_Core0 */
#define RPMSG_MU_BASE       MU2_A
#define RPMSG_MU_IRQn       MU2_A_IRQn
#define RPMSG_MU_IRQHandler MU2_A_IRQHandler
#elif defined(MCXE32B_cm7_core1_SERIES)
/* Building for M7_1 = SECONDARY core, MU2_B side, kMCMGR_Core1 */
#define RPMSG_MU_BASE       MU2_B
#define RPMSG_MU_IRQn       MU2_B_IRQn
#define RPMSG_MU_IRQHandler MU2_B_IRQHandler
#else
#error "MCXE32B rpmsg-lite: unrecognized series macro - expected MCXE32B_cm7_core0_SERIES or MCXE32B_cm7_core1_SERIES"
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
    /* Unused */
    (void)context;
    (void)coreNum;

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

int32_t RPMSG_MU_IRQHandler(void)
{
    mu_isr(RPMSG_MU_BASE);
    SDK_ISR_EXIT_BARRIER;
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
            MU_EnableInterrupts(RPMSG_MU_BASE, MU_GI_INTR(1UL << vector_id));
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
            MU_DisableInterrupts(RPMSG_MU_BASE, MU_GI_INTR(1UL << vector_id));
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
#if defined(MCXE32B_cm7_core1_SERIES)
    (void)MCMGR_TriggerEventForce(kMCMGR_Core0, kMCMGR_RemoteRPMsgEvent, (uint16_t)RL_GET_Q_ID(vector_id));
#elif defined(MCXE32B_cm7_core0_SERIES)
    (void)MCMGR_TriggerEventForce(kMCMGR_Core1, kMCMGR_RemoteRPMsgEvent, (uint16_t)RL_GET_Q_ID(vector_id));
#endif
#else
    /* Write directly into the MU Control Register to trigger General Purpose Interrupt Request (GIR).
       No need to wait until the previous interrupt is processed because the same value
       of the virtqueue ID is used for GIR mask when triggering the ISR for the receiver side.
       The whole queue of received buffers for associated virtqueue is then handled in the ISR
       on the receiver side. */
    (void)MU_TriggerInterrupts(RPMSG_MU_BASE, MU_GI_INTR(1UL << (RL_GET_Q_ID(vector_id))));
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
        NVIC_EnableIRQ(RPMSG_MU_IRQn);
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
        NVIC_DisableIRQ(RPMSG_MU_IRQn);
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

#if defined(RL_USE_DCACHE) && (RL_USE_DCACHE == 1)
/*
 * Cortex-M7 L1 data cache line size is 32 bytes. Clean/invalidate operate on
 * whole cache lines, so the start address is aligned down and the length is
 * rounded up to a multiple of the line size to cover the requested range.
 */
#define PLATFORM_DCACHE_LINE_SIZE (32U)

/**
 * platform_cache_flush
 *
 * Clean (write back) the data cache for the shared memory range so the peer
 * core observes the latest values in physical RAM.
 *
 */
void platform_cache_flush(void *data, uint32_t len)
{
    uint32_t addr  = (uint32_t)data & ~(PLATFORM_DCACHE_LINE_SIZE - 1U);
    uint32_t end   = (uint32_t)data + len;
    uint32_t bytes = end - addr;

    bytes = (bytes + (PLATFORM_DCACHE_LINE_SIZE - 1U)) & ~(PLATFORM_DCACHE_LINE_SIZE - 1U);

    SCB_CleanDCache_by_Addr((uint32_t *)addr, (int32_t)bytes);
}

/**
 * platform_cache_invalidate
 *
 * Invalidate the data cache for the shared memory range so the next read
 * fetches the values written by the peer core from physical RAM.
 *
 */
void platform_cache_invalidate(void *data, uint32_t len)
{
    uint32_t addr  = (uint32_t)data & ~(PLATFORM_DCACHE_LINE_SIZE - 1U);
    uint32_t end   = (uint32_t)data + len;
    uint32_t bytes = end - addr;

    bytes = (bytes + (PLATFORM_DCACHE_LINE_SIZE - 1U)) & ~(PLATFORM_DCACHE_LINE_SIZE - 1U);

    SCB_InvalidateDCache_by_Addr((uint32_t *)addr, (int32_t)bytes);
}
#endif /* defined(RL_USE_DCACHE) && (RL_USE_DCACHE == 1) */


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
