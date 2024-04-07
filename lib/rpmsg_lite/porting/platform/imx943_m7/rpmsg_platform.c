/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>
#include "rpmsg_platform.h"
#include "rpmsg_env.h"

#include "fsl_device_registers.h"
#include "fsl_mu.h"

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

#if (defined(RPMSG_M70_MASTER) || defined(RPMSG_M70_REMOTE) ||  \
    defined(RPMSG_M71_MASTER) || defined(RPMSG_M71_REMOTE))
/* Only used for communication between  CM7 and CM33 with TCM
 * NOTE: this offset is only for TCM offset that share memory is in TCM */
static int32_t vatopa_off = 0x300000;
#endif

static int32_t isr_counter0 = 0;     /* RL_PLATFORM_IMX943_M70_M331_USER_LINK_ID isr counter */
static int32_t isr_counter1 = 0;     /* RL_PLATFORM_IMX943_M71_M70_USER_LINK_ID isr counter */
static int32_t isr_counter2 = 0;     /* RL_PLATFORM_IMX943_M71_M331_USER_LINK_ID isr counter */
static int32_t isr_counter3 = 0;     /* RL_PLATFORM_IMX943_M71_M330_USER_LINK_ID isr counter */
static int32_t isr_counter4 = 0;     /* RL_PLATFORM_IMX943_M7_A55_USER_LINK_ID isr counter */

static int32_t disable_counter0 = 0; /* RL_PLATFORM_IMX943_M70_M331_USER_LINK_ID isr counter */
static int32_t disable_counter1 = 0; /* RL_PLATFORM_IMX943_M71_M70_USER_LINK_ID isr counter */
static int32_t disable_counter2 = 0; /* RL_PLATFORM_IMX943_M71_M331_USER_LINK_ID isr counter */
static int32_t disable_counter3 = 0; /* RL_PLATFORM_IMX943_M71_M330_USER_LINK_ID isr counter */
static int32_t disable_counter4 = 0; /* RL_PLATFORM_IMX943_M7_A55_USER_LINK_ID isr counter */
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
    /* Register ISR to environment layer */
    env_register_isr(vector_id, isr_data);

    /* Prepare the MU Hardware, enable channel 1 interrupt */
    env_lock_mutex(platform_lock);

    switch (RL_GET_COM_ID(vector_id))
    {
        case RL_PLATFORM_IMX943_M331_M70_COM_ID:
            RL_ASSERT(0 <= isr_counter0);
            if (isr_counter0 == 0)
            {
                MU_EnableInterrupts(RPMSG_LITE_M70_M331_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
            }
            isr_counter0++;
            break;
        case RL_PLATFORM_IMX943_M70_M71_COM_ID:
            RL_ASSERT(0 <= isr_counter1);
            if (isr_counter1 == 0)
            {
#if (defined (RPMSG_M70_MASTER) && (RPMSG_M70_MASTER == 1U)) || (defined (RPMSG_M70_REMOTE) && (RPMSG_M70_REMOTE == 1U))
                MU_EnableInterrupts(RPMSG_LITE_M70_M71_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
#endif
#if (defined (RPMSG_M71_MASTER) && (RPMSG_M71_MASTER == 1U)) || (defined (RPMSG_M71_REMOTE) && (RPMSG_M71_REMOTE == 1U))
                MU_EnableInterrupts(RPMSG_LITE_M71_M70_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
#endif
            }
            isr_counter1++;
            break;
        case RL_PLATFORM_IMX943_M331_M71_COM_ID:
            RL_ASSERT(0 <= isr_counter2);
            if (isr_counter2 == 0)
            {
                MU_EnableInterrupts(RPMSG_LITE_M71_M331_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
            }
            isr_counter2++;
            break;
        case RL_PLATFORM_IMX943_M330_M71_COM_ID:
            RL_ASSERT(0 <= isr_counter3);
            if (isr_counter3 == 0)
            {
                MU_EnableInterrupts(RPMSG_LITE_M71_M330_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
            }
            isr_counter3++;
            break;
        case RL_PLATFORM_IMX943_M7_A55_COM_ID:
            RL_ASSERT(0 <= isr_counter4);
            if (isr_counter4 == 0)
            {
#if defined(CPU_MIMX9494AVKXM_cm7_core0)
                MU_EnableInterrupts(RPMSG_LITE_M70_A55_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
#endif
#if defined(CPU_MIMX9494AVKXM_cm7_core1)
                MU_EnableInterrupts(RPMSG_LITE_M71_A55_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
#endif
            }
            isr_counter4++;
            break;
        default:
            /* All the cases have been listed above, the default clause should not be reached. */
            break;
    }

    env_unlock_mutex(platform_lock);

    return 0;
}

int32_t platform_deinit_interrupt(uint32_t vector_id)
{
    /* Prepare the MU Hardware */
    env_lock_mutex(platform_lock);

    switch (RL_GET_COM_ID(vector_id))
    {
        case RL_PLATFORM_IMX943_M331_M70_COM_ID:
            RL_ASSERT(0 < isr_counter0);
            isr_counter0--;
            if (isr_counter0 == 0)
            {
                MU_DisableInterrupts(RPMSG_LITE_M70_M331_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
            }
            break;
        case RL_PLATFORM_IMX943_M70_M71_COM_ID:
            RL_ASSERT(0 < isr_counter1);
            isr_counter1--;
            if (isr_counter1 == 0)
            {
#if (defined (RPMSG_M70_MASTER) && (RPMSG_M70_MASTER == 1U)) || (defined (RPMSG_M70_REMOTE) && (RPMSG_M70_REMOTE == 1U))
                MU_DisableInterrupts(RPMSG_LITE_M70_M71_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
#endif
#if (defined (RPMSG_M71_MASTER) && (RPMSG_M71_MASTER == 1U)) || (defined (RPMSG_M71_REMOTE) && (RPMSG_M71_REMOTE == 1U))
                MU_DisableInterrupts(RPMSG_LITE_M71_M70_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
#endif
            }
            break;
        case RL_PLATFORM_IMX943_M331_M71_COM_ID:
            RL_ASSERT(0 < isr_counter2);
            isr_counter2--;
            if (isr_counter2 == 0)
            {
                MU_DisableInterrupts(RPMSG_LITE_M71_M331_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
            }
            break;
        case RL_PLATFORM_IMX943_M330_M71_COM_ID:
            RL_ASSERT(0 < isr_counter3);
            isr_counter3--;
            if (isr_counter3 == 0)
            {
                MU_DisableInterrupts(RPMSG_LITE_M71_M330_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
            }
            break;
        case RL_PLATFORM_IMX943_M7_A55_COM_ID:
            RL_ASSERT(0 < isr_counter4);
            isr_counter4--;
            if (isr_counter4 == 0)
            {
#if defined(CPU_MIMX9494AVKXM_cm7_core0)
                MU_DisableInterrupts(RPMSG_LITE_M70_A55_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
#endif
#if defined(CPU_MIMX9494AVKXM_cm7_core1)
                MU_DisableInterrupts(RPMSG_LITE_M71_A55_MU, (uint32_t)kMU_Rx0FullInterruptEnable << RPMSG_MU_CHANNEL);
#endif
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

void platform_notify(uint32_t vector_id)
{
    /* Only vring id and queue id is needed in msg */
    uint32_t msg = RL_GEN_MU_MSG(vector_id);

    env_lock_mutex(platform_lock);
    /* As Linux suggests, use MU->Data Channel 1 as communication channel */
    switch (RL_GET_COM_ID(vector_id))
    {
        case RL_PLATFORM_IMX943_M331_M70_COM_ID:
            MU_SendMsg(RPMSG_LITE_M70_M331_MU, RPMSG_MU_CHANNEL, msg);
            break;
        case RL_PLATFORM_IMX943_M70_M71_COM_ID:
#if (defined (RPMSG_M70_MASTER) && (RPMSG_M70_MASTER == 1U)) || (defined (RPMSG_M70_REMOTE) && (RPMSG_M70_REMOTE == 1U))
            MU_SendMsg(RPMSG_LITE_M70_M71_MU, RPMSG_MU_CHANNEL, msg);
#endif
#if (defined (RPMSG_M71_MASTER) && (RPMSG_M71_MASTER == 1U)) || (defined (RPMSG_M71_REMOTE) && (RPMSG_M71_REMOTE == 1U))
            MU_SendMsg(RPMSG_LITE_M71_M70_MU, RPMSG_MU_CHANNEL, msg);
#endif
            break;
        case RL_PLATFORM_IMX943_M331_M71_COM_ID:
            MU_SendMsg(RPMSG_LITE_M71_M331_MU, RPMSG_MU_CHANNEL, msg);
            break;
        case RL_PLATFORM_IMX943_M330_M71_COM_ID:
            MU_SendMsg(RPMSG_LITE_M71_M330_MU, RPMSG_MU_CHANNEL, msg);
            break;
        case RL_PLATFORM_IMX943_M7_A55_COM_ID:
#if defined(CPU_MIMX9494AVKXM_cm7_core0)
            MU_SendMsg(RPMSG_LITE_M70_A55_MU, RPMSG_MU_CHANNEL, msg);
#endif
#if defined(CPU_MIMX9494AVKXM_cm7_core1)
            MU_SendMsg(RPMSG_LITE_M71_A55_MU, RPMSG_MU_CHANNEL, msg);
#endif
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
int32_t MU_E1_A_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(RPMSG_LITE_M71_M331_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(RPMSG_LITE_M71_M331_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX943_M331_M71_COM_ID << 3)));
    }

    return 0;
}
int32_t MU_E3_A_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(RPMSG_LITE_M70_M331_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(RPMSG_LITE_M70_M331_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX943_M331_M70_COM_ID << 3)));
    }

    return 0;
}

int32_t MU7_A_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(RPMSG_LITE_M71_M330_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(RPMSG_LITE_M71_M330_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX943_M330_M71_COM_ID << 3)));
    }

    return 0;
}
int32_t MU18_A_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(RPMSG_LITE_M71_M70_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(RPMSG_LITE_M71_M70_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX943_M70_M71_COM_ID << 3)));
    }

    return 0;
}
int32_t MU18_B_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(RPMSG_LITE_M70_M71_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(RPMSG_LITE_M70_M71_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX943_M70_M71_COM_ID << 3)));
    }

    return 0;
}
int32_t MU11_B_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(RPMSG_LITE_M70_A55_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(RPMSG_LITE_M70_A55_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX943_M7_A55_COM_ID << 3)));
    }

    return 0;
}
int32_t MU13_A_IRQHandler(void)
{
    uint32_t channel;

    if ((((uint32_t)kMU_Rx0FullFlag << RPMSG_MU_CHANNEL) & MU_GetStatusFlags(RPMSG_LITE_M71_A55_MU)) != 0UL)
    {
        channel = MU_ReceiveMsgNonBlocking(RPMSG_LITE_M71_A55_MU, RPMSG_MU_CHANNEL); // Read message from RX register.
        env_isr((uint32_t)((channel >> 16) | (RL_PLATFORM_IMX943_M7_A55_COM_ID << 3)));
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
        case RL_PLATFORM_IMX943_M331_M70_COM_ID:
            RL_ASSERT(0 < disable_counter0);
            disable_counter0--;
            if (disable_counter0 == 0)
            {
                NVIC_EnableIRQ(RPMSG_LITE_M70_M331_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX943_M70_M71_COM_ID:
            RL_ASSERT(0 < disable_counter1);
            disable_counter1--;
            if (disable_counter1 == 0)
            {
#if (defined (RPMSG_M70_MASTER) && (RPMSG_M70_MASTER == 1U)) || (defined (RPMSG_M70_REMOTE) && (RPMSG_M70_REMOTE == 1U))
                NVIC_EnableIRQ(RPMSG_LITE_M70_M71_MU_IRQn);
#endif
#if (defined (RPMSG_M71_MASTER) && (RPMSG_M71_MASTER == 1U)) || (defined (RPMSG_M71_REMOTE) && (RPMSG_M71_REMOTE == 1U))
                NVIC_EnableIRQ(RPMSG_LITE_M71_M70_MU_IRQn);
#endif
            }
            break;
        case RL_PLATFORM_IMX943_M331_M71_COM_ID:
            RL_ASSERT(0 < disable_counter2);
            disable_counter2--;
            if (disable_counter2 == 0)
            {
                NVIC_EnableIRQ(RPMSG_LITE_M71_M331_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX943_M330_M71_COM_ID:
            RL_ASSERT(0 < disable_counter3);
            disable_counter3--;
            if (disable_counter3 == 0)
            {
                NVIC_EnableIRQ(RPMSG_LITE_M71_M330_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX943_M7_A55_COM_ID:
            RL_ASSERT(0 < disable_counter4);
            disable_counter4--;
            if (disable_counter4 == 0)
            {
#if defined(CPU_MIMX9494AVKXM_cm7_core0)
                NVIC_EnableIRQ(RPMSG_LITE_M70_A55_MU_IRQn);
#endif
#if defined(CPU_MIMX9494AVKXM_cm7_core1)
                NVIC_EnableIRQ(RPMSG_LITE_M71_A55_MU_IRQn);
#endif
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
        case RL_PLATFORM_IMX943_M331_M70_COM_ID:
            RL_ASSERT(0 <= disable_counter0);
            disable_counter0++;
            if (disable_counter0 == 0)
            {
                NVIC_DisableIRQ(RPMSG_LITE_M70_M331_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX943_M70_M71_COM_ID:
            RL_ASSERT(0 <= disable_counter1);
            disable_counter1++;
            if (disable_counter1 == 0)
            {
#if (defined (RPMSG_M70_MASTER) && (RPMSG_M70_MASTER == 1U)) || (defined (RPMSG_M70_REMOTE) && (RPMSG_M70_REMOTE == 1U))
                NVIC_DisableIRQ(RPMSG_LITE_M70_M71_MU_IRQn);
#endif
#if (defined (RPMSG_M71_MASTER) && (RPMSG_M71_MASTER == 1U)) || (defined (RPMSG_M71_REMOTE) && (RPMSG_M71_REMOTE == 1U))
                NVIC_DisableIRQ(RPMSG_LITE_M71_M70_MU_IRQn);
#endif
            }
            break;
        case RL_PLATFORM_IMX943_M331_M71_COM_ID:
            RL_ASSERT(0 <= disable_counter2);
            disable_counter2++;
            if (disable_counter2 == 0)
            {
                NVIC_DisableIRQ(RPMSG_LITE_M71_M331_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX943_M330_M71_COM_ID:
            RL_ASSERT(0 <= disable_counter3);
            disable_counter3++;
            if (disable_counter3 == 0)
            {
                NVIC_DisableIRQ(RPMSG_LITE_M71_M330_MU_IRQn);
            }
            break;
        case RL_PLATFORM_IMX943_M7_A55_COM_ID:
            RL_ASSERT(0 <= disable_counter4);
            disable_counter4++;
            if (disable_counter4 == 0)
            {
#if defined(CPU_MIMX9494AVKXM_cm7_core0)
                NVIC_DisableIRQ(RPMSG_LITE_M70_A55_MU_IRQn);
#endif
#if defined(CPU_MIMX9494AVKXM_cm7_core1)
                NVIC_DisableIRQ(RPMSG_LITE_M71_A55_MU_IRQn);
#endif
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
uint32_t platform_vatopa(void *addr)
{
#if !(defined(RPMSG_M70_MASTER) || defined(RPMSG_M70_REMOTE) ||  \
    defined(RPMSG_M71_MASTER) || defined(RPMSG_M71_REMOTE))
    return ((uintptr_t)(char *)addr);
#else
    return ((uint32_t)(char *)addr + vatopa_off);
#endif
}

/**
 * platform_patova
 *
 * Dummy implementation
 *
 */
void *platform_patova(uint32_t addr)
{
#if !(defined(RPMSG_M70_MASTER) || defined(RPMSG_M70_REMOTE) ||  \
    defined(RPMSG_M71_MASTER) || defined(RPMSG_M71_REMOTE))
    return ((void *)(char *)addr);
#else
    return ((void *)(char *)(addr - vatopa_off));
#endif
}

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
    /* M70: Master  M331: Slave*/
#if defined (RPMSG_M70_MASTER) && (RPMSG_M70_MASTER == 1U)
    /* shared m70 memory, convert address for m331*/
    vatopa_off = M70_ALIASED_DTCM_ADDR_OFF_FOR_M33;
    /* interrupt from M331 to M70 */
    MU_Init(RPMSG_LITE_M70_M331_MU);
    NVIC_SetPriority(RPMSG_LITE_M70_M331_MU_IRQn, RPMSG_LITE_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(RPMSG_LITE_M70_M331_MU_IRQn);
#endif

    /* M71: Master  M70: Slave*/
#if defined (RPMSG_M71_MASTER) && (RPMSG_M71_MASTER == 1U)
    /* shared m71 memory, convert address for m331 or m70 */
    vatopa_off = M71_ALIASED_DTCM_ADDR_OFF_FOR_M33;
    /* interrupt from M70 to M71 */
    MU_Init(RPMSG_LITE_M71_M70_MU);
    NVIC_SetPriority(RPMSG_LITE_M71_M70_MU_IRQn, RPMSG_LITE_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(RPMSG_LITE_M71_M70_MU_IRQn);
#endif
#if defined (RPMSG_M70_REMOTE) && (RPMSG_M70_REMOTE == 1U)
    /* shared m71 address */
    vatopa_off = 0;
    /* interrupt from M71 to M70 */
    MU_Init(RPMSG_LITE_M70_M71_MU);
    NVIC_SetPriority(RPMSG_LITE_M70_M71_MU_IRQn, RPMSG_LITE_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(RPMSG_LITE_M70_M71_MU_IRQn);
#endif

    /* M331: Master  M71: Slave*/
#if defined (RPMSG_M71_REMOTE) && (RPMSG_M71_REMOTE == 1U)
    /* shared m71 memory, convert address for m331 or m70 */
    vatopa_off = M71_ALIASED_DTCM_ADDR_OFF_FOR_M33;
    /* interrupt from M331 to M71 */
    MU_Init(RPMSG_LITE_M71_M331_MU);
    NVIC_SetPriority(RPMSG_LITE_M71_M331_MU_IRQn, RPMSG_LITE_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(RPMSG_LITE_M71_M331_MU_IRQn);
#endif
#if !(defined(RPMSG_M70_MASTER) || defined(RPMSG_M70_REMOTE) ||  \
    defined(RPMSG_M71_MASTER) || defined(RPMSG_M71_REMOTE))
#if defined(CPU_MIMX9494AVKXM_cm7_core0)
    /* interrupt from A55 to M70 */
    MU_Init(RPMSG_LITE_M70_A55_MU);
    NVIC_SetPriority(RPMSG_LITE_M70_A55_MU_IRQn, RPMSG_LITE_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(RPMSG_LITE_M70_A55_MU_IRQn);
#endif

#if defined(CPU_MIMX9494AVKXM_cm7_core1)
    /* interrupt from A55 to M71 */
    MU_Init(RPMSG_LITE_M71_A55_MU);
    NVIC_SetPriority(RPMSG_LITE_M71_A55_MU_IRQn, RPMSG_LITE_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(RPMSG_LITE_M71_A55_MU_IRQn);
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
#if defined (RPMSG_M70_MASTER) && (RPMSG_M70_MASTER == 1U)
    MU_Deinit(RPMSG_LITE_M70_M331_MU);
#endif

#if defined (RPMSG_M70_REMOTE) && (RPMSG_M70_REMOTE == 1U)
    MU_Deinit(RPMSG_LITE_M70_M71_MU);
#endif

#if defined (RPMSG_M71_REMOTE) && (RPMSG_M71_REMOTE == 1U)
    MU_Deinit(RPMSG_LITE_M71_M331_MU);
#endif

#if defined (RPMSG_M71_MASTER) && (RPMSG_M71_MASTER == 1U)
    MU_Deinit(RPMSG_LITE_M71_M70_MU);
#endif

#if !(defined(RPMSG_M70_MASTER) || defined(RPMSG_M70_REMOTE) ||  \
    defined(RPMSG_M71_MASTER) || defined(RPMSG_M71_REMOTE))
#if defined(CPU_MIMX9494AVKXM_cm7_core0)
    MU_Deinit(RPMSG_LITE_M70_A55_MU);
#endif

#if defined(CPU_MIMX9494AVKXM_cm7_core1)
    MU_Deinit(RPMSG_LITE_M71_A55_MU);
#endif

#endif
    /* Delete lock used in multi-instanced RPMsg */
    env_delete_mutex(platform_lock);
    platform_lock = ((void *)0);
    return 0;
}
