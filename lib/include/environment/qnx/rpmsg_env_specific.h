/*
 * Copyright 2021,2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**************************************************************************
 * FILE NAME
 *
 *       rpmsg_env_specific.h
 *
 * DESCRIPTION
 *
 *       This file contains QNX specific constructions.
 *
 **************************************************************************/
#ifndef RPMSG_ENV_SPECIFIC_H_
#define RPMSG_ENV_SPECIFIC_H_

#include <stdint.h>
#include "rpmsg_default_config.h"

typedef struct rpmsg_env_init
{
    void *user_input; /* Pointer to user init cfg */
    uint32_t pa;      /* Physical address of memory pool reserved for rpmsg */
    void *va;         /* Virtual address of the memory pool */
} rpmsg_env_init_t;

typedef struct
{
    uint32_t src;
    void *data;
    uint32_t len;
} rpmsg_queue_rx_cb_data_t;

#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
#error "This RPMsg-Lite port requires RL_USE_STATIC_API set to 0"
#endif

#endif /* RPMSG_ENV_SPECIFIC_H_ */
