/*
 * Copyright 2020-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <string.h>

#include "rpmsg_platform.h"
#include "rpmsg_env.h"

#include "fsl_device_registers.h"
#include "fsl_imu.h"

#if defined(RL_USE_MCMGR_IPC_ISR_HANDLER) && (RL_USE_MCMGR_IPC_ISR_HANDLER == 1)
#include "mcmgr.h"
#endif

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

#define APP_MU_IRQ_PRIORITY (3U)

#if defined(IMU_CPU_INDEX) && (IMU_CPU_INDEX == 1U)
#define APP_MU_IRQn  RF_IMU0_IRQn
#define APP_IMU_LINK kIMU_LinkCpu1Cpu2
#elif defined(IMU_CPU_INDEX) && (IMU_CPU_INDEX == 2U)
#define APP_MU_IRQn  CPU2_MSG_RDY_INT_IRQn
#define APP_IMU_LINK kIMU_LinkCpu2Cpu1
#endif

#if defined(IMU_CPU_INDEX) && (IMU_CPU_INDEX == 1U)

/**
 * platform_vatopa
 *
 * Translate NBU addresses to CM33 addresses
 *
 */
uintptr_t platform_vatopa(void *addr)
{
    return ((uintptr_t)(char *)addr);
}

/**
 * platform_patova
 *
 * Translate CM33 addresses to NBU addresses
 *
 */
void *platform_patova(uintptr_t addr)
{
    return ((void *)(char *)addr);
}

#elif defined(IMU_CPU_INDEX) && (IMU_CPU_INDEX == 2U)
/**
 * platform_vatopa
 *
 * Translate NBU addresses to CM3 addresses
 *
 */
uintptr_t platform_vatopa(void *addr)
{
    return (((uintptr_t)(char *)addr & 0x0000FFFFu) + 0x489C0000u);
}

/**
 * platform_patova
 *
 * Translate CM33 addresses to NBU addresses
 *
 */
void *platform_patova(uintptr_t addr)
{
    return ((void *)(char *)((addr & 0x0000FFFFu) + 0xB0000000u));
}
#endif
