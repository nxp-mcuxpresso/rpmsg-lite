/*
 * Copyright 2019-2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*${header:start}*/
#include <xtensa/config/core.h>
#ifdef FSL_RTOS_XOS
#include <xtensa/xos.h>
#endif
#include "app.h"
#include "board.h"
#include "fsl_common.h"
#include "fsl_inputmux.h"
#include "pin_mux.h"
/*${header:end}*/

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*${macro:start}*/
/*${macro:end}*/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/*${variable:start}*/
extern int NonCacheable_start, NonCacheable_end;
extern int NonCacheable_init_start, NonCacheable_init_end;
/*${variable:end}*/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*${function:start}*/
#ifdef FSL_RTOS_XOS
static void XOS_Init(void)
{
    xos_set_clock_freq(XOS_CLOCK_FREQ);

    xos_start_system_timer(-1, 0);
}
#endif
void BOARD_InitHardware(void)
{
    /* Disable DSP cache for noncacheable sections. */
    xthal_set_region_attribute((uint32_t *)0x20400000, 30000, XCHAL_CA_BYPASS, 0);
    CLOCK_SetXtalFreq(BOARD_XTAL_SYS_CLK_HZ); /* Note: need tell clock driver the frequency of OSC. */

#ifdef FSL_RTOS_XOS
    XOS_Init();
#endif
    BOARD_InitBootPins();
    BOARD_InitDebugConsole();
}

/*${function:end}*/
