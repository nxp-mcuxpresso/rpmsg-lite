/*
 * Copyright 2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _APP_H_
#define _APP_H_

/*${header:start}*/
#include "dsp_config.h"
/*${header:end}*/

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*${macro:start}*/
#define SH_MEM_NOT_TAKEN_FROM_LINKER

#define RPMSG_LITE_LINK_ID    (RL_PLATFORM_IMXRT700_M33_0_HIFI4_LINK_ID)
#define RPMSG_LITE_SHMEM_BASE (void *)0x20200000
/*${macro:end}*/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*${prototype:start}*/
void BOARD_InitHardware(void);
/*${prototype:end}*/

#endif /* _APP_H_ */
