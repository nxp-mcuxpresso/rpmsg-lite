/*
 * Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "env.h"

#include "fsl_device_registers.h"
#include "fsl_mailbox.h"

void MAILBOX_IRQHandler(void)
{
    mailbox_cpu_id_t cpu_id;
#if defined(__CM4_CMSIS_VERSION)
    cpu_id = kMAILBOX_CM4;
#else
    cpu_id = kMAILBOX_CM0Plus;
#endif

    uint32_t value = MAILBOX_GetValue(MAILBOX, cpu_id);

    if (value & 0x01)
    {
        env_isr(0);
        MAILBOX_ClearValueBits(MAILBOX, cpu_id, 0x01);
    }
    if (value & 0x02)
    {
        env_isr(1);
        MAILBOX_ClearValueBits(MAILBOX, cpu_id, 0x02);
    }
}

int platform_init_interrupt(int vq_id, void *isr_data)
{
/* Do any HW related interrupt initialization here */
#if defined(__CM4_CMSIS_VERSION)
    NVIC_SetPriority(MAILBOX_IRQn, 5);
#else
    NVIC_SetPriority(MAILBOX_IRQn, 2);
#endif

    /* Register ISR to environment layer */
    env_register_isr(vq_id, isr_data);
    return 0;
}

int platform_deinit_interrupt(int vq_id)
{
    /* Do any HW related interrupt deinitialization here */
    NVIC_DisableIRQ(MAILBOX_IRQn);

    /* Unregister ISR from environment layer */
    /* nothing */
    return 0;
}

void platform_notify(int vq_id)
{
    switch (RL_GET_CORE_ID(vq_id))
    {
        case 0:
#if defined(__CM4_CMSIS_VERSION)
            MAILBOX_SetValueBits(MAILBOX, kMAILBOX_CM0Plus, (1 << RL_GET_Q_ID(vq_id)));
#else
            MAILBOX_SetValueBits(MAILBOX, kMAILBOX_CM4, (1 << RL_GET_Q_ID(vq_id)));
#endif
            return;

        default:
            return;
    }
}

#define PLATFORM_DISABLE_COUNTERS (2) /* Change for multiple remote cores */
static int disable_counters[PLATFORM_DISABLE_COUNTERS] = { 0 };
static int disable_counter_all = 0;

/**
 * platform_time_delay
 *
 * @param num_msec Delay time in ms.
 *
 * This is not an accurate delay, it ensures at least num_msec passed when return.
 */
void platform_time_delay(int num_msec)
{
    uint32_t loop;

    /* Recalculate the CPU frequency */
    SystemCoreClockUpdate();

    /* Calculate the CPU loops to delay, each loop has 6 cycles */
    loop = SystemCoreClock / 6 / 1000 * num_msec;

    /* There's some difference among toolchains */
    while (loop)
    {
        __NOP();
        loop--;
    }

#if 0
    num_msec++;
    /* Wait desired time */
    while (num_msec > 0)
    {
        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
        {
            num_msec--;
        }
    }
#endif
}

/**
 * platform_in_isr
 *
 * Return whether CPU is processing IRQ
 *
 * @return True for IRQ, false otherwise.
 *
 */
int platform_in_isr(void)
{
    return ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0);
}

/**
 * platform_interrupt_enable
 *
 * Enable peripheral-related interrupt with passed priority and type.
 *
 * @param vq_id Vector ID that need to be converted to IRQ number
 * @param trigger_type IRQ active level
 * @param trigger_type IRQ priority
 *
 * @return vq_id. Return value is never checked..
 *
 */
int platform_interrupt_enable(unsigned int vq_id)
{
    assert(vq_id < PLATFORM_DISABLE_COUNTERS);
    assert(0 < disable_counters[vq_id]);
    disable_counters[vq_id]--;
    if (0 == disable_counters[vq_id])
        NVIC_EnableIRQ(MAILBOX_IRQn);
    return (vq_id);
}

/**
 * platform_interrupt_disable
 *
 * Disable peripheral-related interrupt.
 *
 * @param vq_id Vector ID that need to be converted to IRQ number
 *
 * @return vq_id. Return value is never checked.
 *
 */
int platform_interrupt_disable(unsigned int vq_id)
{
    assert(vq_id < PLATFORM_DISABLE_COUNTERS);
    assert(0 <= disable_counters[vq_id]);
    if (0 == disable_counters[vq_id])
        NVIC_DisableIRQ(MAILBOX_IRQn);
    disable_counters[vq_id]++;
    return (vq_id);
}

/**
 * platform_interrupt_enable_all
 *
 * Enable all platform-related interrupts.
 *
 */
void platform_interrupt_enable_all(void)
{
    assert(0 < disable_counter_all);
    disable_counter_all--;
    if (0 == disable_counter_all)
        platform_interrupt_enable(0);
}

/**
 * platform_interrupt_disable_all
 *
 * Enable all platform-related interrupts.
 *
 */
void platform_interrupt_disable_all(void)
{
    assert(0 <= disable_counter_all);
    if (0 == disable_counter_all)
        platform_interrupt_disable(0);
    disable_counter_all++;
}

/**
 * platform_map_mem_region
 *
 * Dummy implementation
 *
 */
void platform_map_mem_region(unsigned int vrt_addr, unsigned int phy_addr, unsigned int size, unsigned int flags)
{
}

/**
 * platform_cache_all_flush_invalidate
 *
 * Dummy implementation
 *
 */
void platform_cache_all_flush_invalidate()
{
}

/**
 * platform_cache_disable
 *
 * Dummy implementation
 *
 */
void platform_cache_disable()
{
}

/**
 * platform_vatopa
 *
 * Dummy implementation
 *
 */
unsigned long platform_vatopa(void *addr)
{
    return ((unsigned long)addr);
}

/**
 * platform_patova
 *
 * Dummy implementation
 *
 */
void *platform_patova(unsigned long addr)
{
    return ((void *)addr);
}

/**
 * platform_init
 *
 * platform/environment init
 */
int platform_init(void)
{
    MAILBOX_Init(MAILBOX);

#if 0
    /* Disable SysTick timer */
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    /* Initialize Reload value to 1ms */
    SysTick->LOAD = CLOCK_GetFreq(kCLOCK_CoreSysClk) / 1000;
    /* Set clock source to processor clock */
    SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
    /* Enable SysTick timer */
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
#endif

    return 0;
}

/**
 * platform_deinit
 *
 * platform/environment deinit process
 */
int platform_deinit(void)
{
/* Important for LPC5411x - do not deinit mailbox, if there
   is a pending ISR on the other core! */
#if defined(__CM4_CMSIS_VERSION)
    while (0 != MAILBOX_GetValue(MAILBOX, kMAILBOX_CM0Plus))
        ;
#else
    while (0 != MAILBOX_GetValue(MAILBOX, kMAILBOX_CM4))
        ;
#endif

    MAILBOX_Deinit(MAILBOX);
    return 0;
}
