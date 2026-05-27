/*
 * Copyright 2018-2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*${header:start}*/
#include "fsl_debug_console.h"
#include "app.h"
#include "clock_config.h"
#include "pin_mux.h"
#include "board.h"
#include "fsl_dsp.h"
/*${header:end}*/

/*${function:start}*/

void BOARD_InitHardware(void)
{
    BOARD_ConfigMPU();
    BOARD_InitPins();
    BOARD_InitBootClocks();

    BOARD_InitDebugConsole();

    BOARD_InitAHBSC();


    /* Powerup all the SRAM partitions. */
    PMC0->PDRUNCFG2 &= ~0x3FFC0000;
    PMC0->PDRUNCFG3 &= ~0x3FFC0000;
}

/*${function:end}*/
