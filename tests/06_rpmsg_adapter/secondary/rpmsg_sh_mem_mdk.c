/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * MDK (armlink) does not support --defsym like GCC or --config_def like IAR.
 * The scatter file places rpmsg_sh_mem_section at the shared-memory base address,
 * so defining rpmsg_sh_mem_start here gives it the correct link-time address.
 * On GCC/IAR these symbols are provided by linker script mechanisms instead.
 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
#include <stdint.h>

__attribute__((section("rpmsg_sh_mem_section"), used))
uint32_t rpmsg_sh_mem_start[1];

__attribute__((section("rpmsg_sh_mem_section"), used))
uint32_t rpmsg_sh_mem_end[1];
#endif
