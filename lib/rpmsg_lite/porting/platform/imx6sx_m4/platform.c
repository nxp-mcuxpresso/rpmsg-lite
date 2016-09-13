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

#include "mu_imx.h"

static int _init_interrupt(struct proc_vring *vring_hw)
{
    /* Register ISR*/
    env_register_isr(vring_hw->intr_info.vect_id, vring_hw, platform_isr);

    /* Prepare the MU Hardware, enable channel 1 interrupt */
    MU_EnableRxFullInt(MUB, RPMSG_MU_CHANNEL);

    return 0;
}

static int _deinit_interrupt(struct proc_vring *vring_hw)
{
    /* Prepare the MU Hardware, enable channel 1 interrupt */
    MU_DisableRxFullInt(MUB, RPMSG_MU_CHANNEL);

    return 0;
}

static void _notify(int cpu_id, struct proc_intr *intr_info)
{
    /* As Linux suggests, use MU->Data Channle 1 as communication channel */
    uint32_t msg = (intr_info->vect_id) << 16;
    MU_SendMsg(MUB, RPMSG_MU_CHANNEL, msg);
}

static int _boot_cpu(int cpu_id, unsigned int load_addr)
{
    /* not imlemented */
    assert(0);
    return 0;
}

static void _shutdown_cpu(int cpu_id)
{
    /* not imlemented */
    assert(0);
}

struct hil_platform_ops proc_ops = {
    .enable_interrupt = _init_interrupt,
    .disable_interrupt = _deinit_interrupt,
    .notify = _notify,
    .boot_cpu = _boot_cpu,
    .shutdown_cpu = _shutdown_cpu,
};

/*
 * MU Interrrupt RPMsg handler
 */
void rpmsg_handler(void)
{
    uint32_t msg, channel;

    if (MU_TryReceiveMsg(MUB, RPMSG_MU_CHANNEL, &msg) == kStatus_MU_Success)
    {
        channel = msg >> 16;
        env_isr(channel);
    }

    return;
}

#define PLATFORM_DISABLE_COUNTERS 2
static int disable_counters[PLATFORM_DISABLE_COUNTERS] = { 0 };
static int disable_counter_all = 0;

/*!
 * platform_time_delay
 *
 * @param num_msec - delay time in ms.
 *
 * This is not an accurate delay, it ensures at least num_msec passed when return.
 */
void platform_time_delay(int num_msec)
{
    uint32_t loop;

    /* Recalculate the CPU frequency */
    SystemCoreClockUpdate();

    /* Calculate the CPU loops to delay, each loop has 3 cycles */
    loop = SystemCoreClock / 3 / 1000 * num_msec;

    /* There's some difference among toolchains, 3 or 4 cycles each loop */
    while (loop)
    {
        __NOP();
        loop--;
    }
}

/*!
 * platform_isr
 *
 * RPMSG platform IRQ callback
 *
 */
void platform_isr(int vect_id, void *data)
{
    hil_isr(((struct proc_vring *)data));
}

/*!
 * platform_in_isr
 *
 * Return whether CPU is processing IRQ
 *
 * @return - true for IRQ, false otherwise.
 *
 */
int platform_in_isr(void)
{
    return ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0);
}

/*!
 * platform_interrupt_enable
 *
 * Enable peripheral-related interrupt with passed priority and type.
 *
 * @param vector_id - vector ID that need to be converted to IRQ number
 * @param trigger_type - IRQ active level
 * @param trigger_type - IRQ priority
 *
 * @return - vector_id. Return value is never checked..
 *
 */
int platform_interrupt_enable(unsigned int vector_id, unsigned int trigger_type, unsigned int priority)
{
    assert(vector_id < PLATFORM_DISABLE_COUNTERS);
    assert(0 < disable_counters[vector_id]);
    disable_counters[vector_id]--;
    // channels use the same NVIC vector
    // enable only if all counters are zero
    for (int i = 0; i < PLATFORM_DISABLE_COUNTERS; i++)
    {
        if (disable_counters[i])
            return (vector_id);
    }
    NVIC_EnableIRQ(MU_M4_IRQn);
    return (vector_id);
}

/*!
 * platform_interrupt_disable
 *
 * Disable peripheral-related interrupt.
 *
 * @param vector_id - vector ID that need to be converted to IRQ number
 *
 * @return - vector_id. Return value is never checked.
 *
 */
int platform_interrupt_disable(unsigned int vector_id)
{
    assert(vector_id < PLATFORM_DISABLE_COUNTERS);
    assert(0 <= disable_counters[vector_id]);
    int disabled = 0;
    // channels use the same NVIC vector
    // if one counter is set - the interrupts are disabled
    for (int i = 0; i < PLATFORM_DISABLE_COUNTERS; i++)
    {
        if (disable_counters[i])
        {
            disabled = 1;
            break;
        }
    }
    // if not disabled - disable interrutps
    if (!disabled)
        NVIC_DisableIRQ(MU_M4_IRQn);
    disable_counters[vector_id]++;
    return (vector_id);
}

/*!
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
        platform_interrupt_enable(0, 0, 0);
}

/*!
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

/*!
 * platform_map_mem_region
 *
 * Dummy implementation
 *
 */
void platform_map_mem_region(unsigned int vrt_addr, unsigned int phy_addr, unsigned int size, unsigned int flags)
{
}

/*!
 * platform_cache_all_flush_invalidate
 *
 * Dummy implementation
 *
 */
void platform_cache_all_flush_invalidate()
{
}

/*!
 * platform_cache_disable
 *
 * Dummy implementation
 *
 */
void platform_cache_disable()
{
}

/*!
 * platform_vatopa
 *
 * Dummy implementation
 *
 */
unsigned long platform_vatopa(void *addr)
{
    return ((unsigned long)addr);
}

/*!
 * platform_patova
 *
 * Dummy implementation
 *
 */
void *platform_patova(unsigned long addr)
{
    return ((void *)addr);
}

/*!
 * platform_init
 *
 * platform/environment init
 */
int platform_init(void)
{
    return 0;
}

/*!
 * platform_deinit
 *
 * platform/environment deinit process
 */
int platform_deinit(void)
{
    return 0;
}
