/*
 * Copyright 2021 NXP
 * All rights reserved.
 *
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
 *       This file contains rt-thread specific constructions.
 *
 **************************************************************************/
#ifndef RPMSG_ENV_SPECIFIC_H_
#define RPMSG_ENV_SPECIFIC_H_

#include <rtthread.h>

#include <stdint.h>
#include "rpmsg_default_config.h"

typedef struct
{
    uint32_t src;
    void *data;
    uint32_t len;
} rpmsg_queue_rx_cb_data_t;

#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
typedef struct rt_mutex LOCK_STATIC_CONTEXT;
typedef struct rt_messagequeue rpmsg_static_queue_ctxt;
#endif

#endif /* RPMSG_ENV_SPECIFIC_H_ */
