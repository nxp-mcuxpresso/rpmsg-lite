/*
 * Copyright 2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _APP_CORECM33_H_
#define _APP_CORECM33_H_

/*${header:start}*/
#include "dsp_support.h"
#include "fsl_dsp.h"
/*${header:end}*/

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*${macro:start}*/
#define SH_MEM_NOT_TAKEN_FROM_LINKER

#define RPMSG_LITE_LINK_ID    (RL_PLATFORM_IMXRT700_M33_0_HIFI4_LINK_ID)
#define RPMSG_LITE_SHMEM_BASE (void *)0x20200000
#define RPMSG_LITE_SHMEM_SIZE (0x1800)

/*${macro:end}*/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*${prototype:start}*/
void BOARD_InitHardware(void);
/*${prototype:end}*/

#endif /* _APP_CORECM33_H_ */
