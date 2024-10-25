/*
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * Copyright 2016-2024 NXP
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rpmsg_platform.h"
#include "rpmsg_env.h"
#include <pthread.h>
#include <sys/neutrino.h>
#include <hw/imx8_mu_drv.h>
#include <unistd.h>
#include "rpmsg_config.h"

#if (!defined(RL_USE_ENVIRONMENT_CONTEXT)) || (RL_USE_ENVIRONMENT_CONTEXT != 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 1"
#endif

#define RECV_THREAD_READY 1
#define RECV_THREAD_FAIL  2

typedef struct platform_context
{
    uint32_t mu_base;
    uint32_t mu_irq;
    void *mu_handle;
    pthread_t thread;
    volatile uint32_t ready;
    void *env_ctx;
} platform_context_t;

/**
 * MU receive thread started from platform_init()
 *
 * @param data  Pointer to platform context
 */
void *platform_recv_thread(void *data)
{
    uint32_t read;
    int32_t status;
    int32_t ready_flag      = RECV_THREAD_READY;
    platform_context_t *ctx = (platform_context_t *)data;

    /* Set thread privileges for MU library */
    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) != 0)
    {
        ready_flag = RECV_THREAD_FAIL;
    }
    /* imx_mu_init() may fail in case thread privileges were not correctly set */
    if (ctx && (ready_flag != RECV_THREAD_FAIL))
    {
        /* Initialize Messaging Unit */
        ctx->mu_handle = imx_mu_init(ctx->mu_base, ctx->mu_irq);
        if (ctx->mu_handle == ((void *)0))
        {
            /* We should report fail */
            ready_flag = RECV_THREAD_FAIL;
        }
    }
    else
    {
        ready_flag = RECV_THREAD_FAIL;
    }
    /* Wake up the parent and report thread status */
    do
    {
        if ((status = pthread_sleepon_lock()) == EOK)
        {
            ctx->ready = ready_flag;
            pthread_sleepon_signal(&ctx->ready);
            pthread_sleepon_unlock();
        }
        /* This is unusual but in case the lock fails try it again */
    } while (status != EOK);
    /* Exit in case of fail */
    if (ready_flag == RECV_THREAD_FAIL)
    {
        return ((void *)0);
    }
    /* Loop waiting on MU RX */
    /* coverity[no_escape] - Suppress coverity INFINITE_LOOP error */
    while (1)
    {
        /* Continuously receive data in interrupt mode */
        if (imx_mu_read(ctx->mu_handle, RPMSG_MU_CHANNEL, &read, 0, 0) == 0)
        {
            env_isr(ctx->env_ctx, read >> 16);
        }
    }
}

/**
 * Used to notify other MU side
 *
 * @param platform_context  Pointer to platform context
 * @param vector_id         Vector ID.
 */
void platform_notify(void *platform_context, uint32_t vector_id)
{
    platform_context_t *ctx = platform_context;
    /* As Linux suggests, use MU->Data Channel 1 as communication channel */
    uint32_t msg = (RL_GET_Q_ID(vector_id)) << 16;

    if (ctx)
    {
        if (ctx->mu_handle)
        {
            while (imx_mu_send(ctx->mu_handle, RPMSG_MU_CHANNEL, msg, 1000, IMX_MU_FLAG_BLOCK) != 0)
            {
                /* Sleep some time */
                delay(1);
            }
        }
    }
}

/**
 * platform_in_isr
 *
 * Return whether CPU is processing IRQ
 *
 * @return True for IRQ, false otherwise.
 *
 */
int32_t platform_in_isr(void)
{
    /* We work in thread */
    return 0;
}

/**
 * platform_interrupt_enable
 *
 * Enable peripheral-related interrupt with passed priority and type.
 *
 * @param vector_id Vector ID that need to be converted to IRQ number
 *
 * @return Return value is never checked.
 *
 */
int32_t platform_interrupt_enable(uint32_t vector_id)
{
    /* We work in thread */
    return 0;
}

/**
 * platform_interrupt_disable
 *
 * Disable peripheral-related interrupt.
 *
 * @param vector_id Vector ID that need to be converted to IRQ number
 *
 * @return Return value is never checked.
 *
 */
int32_t platform_interrupt_disable(uint32_t vector_id)
{
    /* We work in thread */
    return 0;
}

/**
 * platform_map_mem_region
 *
 * Dummy implementation
 *
 */
void platform_map_mem_region(uint32_t va, uint32_t pa, uint32_t size, uint32_t flags)
{
    fprintf(stderr, "%s unsupported\n", __FUNCTION__);
}

/**
 * platform_cache_all_flush_invalidate
 *
 * Dummy implementation
 *
 */
void platform_cache_all_flush_invalidate(void)
{
    fprintf(stderr, "%s unsupported\n", __FUNCTION__);
}

/**
 * platform_cache_disable
 *
 * Dummy implementation
 *
 */
void platform_cache_disable(void)
{
    fprintf(stderr, "%s unsupported\n", __FUNCTION__);
}

/**
 * platform_cache_flush
 *
 * Empty implementation
 *
 */
void platform_cache_flush(void *data, uint32_t len)
{
}

/**
 * platform_cache_invalidate
 *
 * Empty implementation
 *
 */
void platform_cache_invalidate(void *data, uint32_t len)
{
}

/**
 * platform_init
 *
 * @param platform_context Pointer where to store address of platform context
 * @param env_context      Pointer to existing env_context to use it when calls env_isr()
 * @param platform_init    Pointer to platform initialization structure
 *
 */
int32_t platform_init(void **platform_context, void *env_context, void *platform_init)
{
    int32_t status;
    rpmsg_platform_init_data_t *init = platform_init;
    platform_context_t *ctx          = env_allocate_memory(sizeof(platform_context_t));

    if (!ctx || !init)
    {
        if (ctx)
        {
            env_free_memory(ctx);
        }
        return -1;
    }
    ctx->mu_base = init->mu_base;
    ctx->mu_irq  = init->mu_irq;
    ctx->ready   = 0;
    /* Link pointer to env context */
    ctx->env_ctx = env_context;

    /* Start the receive thread */
    if (pthread_create(&ctx->thread, ((void *)0), platform_recv_thread, ctx) != 0)
    {
        env_free_memory(ctx);
        return -1;
    }
    do
    {
        /* Sleep waiting on the platform_recv_thread() */
        if ((status = pthread_sleepon_lock()) == EOK)
        {
            while (!ctx->ready)
            {
                /* Sleep and wait */
                pthread_sleepon_wait(&ctx->ready);
            }
            pthread_sleepon_unlock();
        }
        else
        {
            /* If the lock fails it means we touched it in same time as recv thread */
            delay(1);
        }
    } while (status != EOK);

    /* Check the returned status */
    if (ctx->ready != RECV_THREAD_READY)
    {
        /* Abort platform initialization */
        env_free_memory(ctx);
        return -1;
    }

    *platform_context = ctx;
    return 0;
}

/**
 * platform_deinit
 *
 * platform/environment deinit process
 */
int32_t platform_deinit(void *platform_context)
{
    platform_context_t *ctx = platform_context;
    pthread_cancel(ctx->thread);
    imx_mu_deinit(ctx->mu_handle);
    env_free_memory(ctx);
    return 0;
}
