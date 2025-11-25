/*
 * Copyright 2021 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DSP_CONFIG_H_
#define _DSP_CONFIG_H_

/* Address of RAM, where the image for DSP should be copied */
/* These addresses are accessed by the ARM core and aliased to M33 code memory */
#define DSP_RESET_ADDRESS (uint32_t *)0x00180000
#define DSP_TEXT_ADDRESS  (uint32_t *)0x00180400
#define DSP_SRAM_ADDRESS  (uint32_t *)0x00080000

/* Inter processor communication common RAM */
/* This address is accessed by both cores, and aliased to M33 data memory */
#define RPMSG_LITE_LINK_ID    (RL_PLATFORM_IMXRT500_LINK_ID)
#define RPMSG_LITE_SHMEM_BASE (void *)0x20070000
#define RPMSG_LITE_SHMEM_SIZE (0x1800)

#endif
