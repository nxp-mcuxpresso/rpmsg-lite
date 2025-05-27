/*
 * Copyright (c) 2014, Mentor Graphics Corporation
 * Copyright (c) 2015 Xilinx, Inc.
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * Copyright 2016-2025 NXP
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**************************************************************************
 * FILE NAME
 *
 *       rpmsg_env_qnx.c
 *
 *
 * DESCRIPTION
 *
 *       This file is QNX Implementation of env layer.
 *
 *
 **************************************************************************/

#include "rpmsg_env.h"
#include "rpmsg_lite.h"
#include "rpmsg_platform.h"
#include "virtqueue.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>

#if __PTR_BITS__ > 32
#include <fcntl.h>
#include <aarch64/inline.h>
#else
#include <arm/inline.h>
#endif

#include <sys/imx_rpmsg_lite.h>

/* Max supported ISR counts */
#define ISR_COUNT (32U)

#if (!defined(RL_USE_ENVIRONMENT_CONTEXT)) || (RL_USE_ENVIRONMENT_CONTEXT != 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 1"
#endif

/**
 * Structure to keep track of registered ISR's.
 */
struct isr_info
{
    void *data;
    volatile uint32_t enabled;
};

/**
 * Structure to hold queue information
 */
typedef struct env_queue
{
    mqd_t mqd;
    size_t msg_len;
} env_queue_t;

/**
 * Env. context structure
 */
typedef struct env_context
{
    void *platform_context;               /* Pointer to platform context */
    uint32_t pa;                          /* Physical address of memory pool reserved for rpmsg */
    void *va;                             /* Virtual address of the memory pool */
    struct isr_info isr_table[ISR_COUNT]; /* Table with registered Virt. queue data */
} env_context_t;

/**
 * Returns pointer to platform context.
 *
 * @param env_context Pointer to env. context
 *
 * @return Pointer to platform context
 */
void *env_get_platform_context(void *env_context)
{
    env_context_t *env = env_context;
    return env->platform_context;
}

/*!
 * env_wait_for_link_up
 *
 * Wait until the link_state parameter of the rpmsg_lite_instance is set.
 * Busy loop implementation, timeout_ms parameter, to be replaced by events.
 *
 */
uint32_t env_wait_for_link_up(volatile uint32_t *link_state, uint32_t link_id, uint32_t timeout_ms)
{
    uint32_t tick_count = 0U;
    uint32_t tick_temp;

    while (*link_state != 1U)
    {
        env_sleep_msec(RL_MS_PER_INTERVAL);

        if (RL_BLOCK != timeout_ms)
        {
            tick_temp = tick_count + (uint32_t)RL_MS_PER_INTERVAL;

            if ((tick_temp < tick_count) || (tick_temp >= timeout_ms))
            {
                return 0U;
            }

            tick_count = tick_temp;
        }
    }

    return 1U;
}

/*!
 * env_tx_callback
 *
 * Set event to notify task waiting in env_wait_for_link_up().
 *
 */
void env_tx_callback(uint32_t link_id)
{
}

/*!
 * env_init
 *
 * Initializes OS/BM environment.
 *
 */
int32_t env_init(void **env_context, void *env_init_data)
{
    rpmsg_env_init_t *init = env_init_data;
    imx_rpmsg_env_cfg_t *user_cfg;

    if (init != ((void *)0))
    {
        user_cfg           = init->user_input;
        env_context_t *ctx = env_allocate_memory(sizeof(env_context_t));
        if (ctx == ((void *)0))
        {
            return -1;
        }
        /* Save virtual and phy address of mmaped memory region */
        ctx->pa = init->pa;
        ctx->va = init->va;
        /* Initialize platform, dereference user_input to get platform cfg address */
        if (platform_init(&ctx->platform_context, ctx, user_cfg ? user_cfg->platform_cfg : ((void *)0)) != 0)
        {
            env_free_memory(ctx);
            return -1;
        }
        *env_context = ctx;
        return 0;
    }
    return -1;
}

/*!
 * env_deinit
 *
 * Uninitializes OS/BM environment.
 *
 * @returns - execution status
 */
int32_t env_deinit(void *env_context)
{
    env_context_t *ctx = env_context;
    platform_deinit(ctx->platform_context);
    env_free_memory(ctx);
    return 0;
}

/*!
 * env_allocate_memory - implementation
 *
 * @param size
 */
void *env_allocate_memory(uint32_t size)
{
    return malloc(size);
}

/*!
 * env_free_memory - implementation
 *
 * @param ptr
 */
void env_free_memory(void *ptr)
{
    free(ptr);
}

/*!
 *
 * env_memset - implementation
 *
 * @param ptr
 * @param value
 * @param size
 */
void env_memset(void *ptr, int32_t value, uint32_t size)
{
    /* Explicitly convert value to unsigned char range to ensure consistent behavior */
    (void)memset(ptr, (unsigned char)(value & 0xFF), size);
}

/*!
 *
 * env_memcpy - implementation
 *
 * @param dst
 * @param src
 * @param len
 */
void env_memcpy(void *dst, void const *src, uint32_t len)
{
    (void)memcpy(dst, src, len);
}

/*!
 *
 * env_strcmp - implementation
 *
 * @param dst
 * @param src
 */

int32_t env_strcmp(const char *dst, const char *src)
{
    return (strcmp(dst, src));
}

/*!
 *
 * env_strncpy - implementation
 *
 * @param dest
 * @param src
 * @param len
 */
void env_strncpy(char *dest, const char *src, uint32_t len)
{
    (void)strncpy(dest, src, len);
}

/*!
 *
 * env_strncmp - implementation
 *
 * @param dest
 * @param src
 * @param len
 */
int32_t env_strncmp(char *dest, const char *src, uint32_t len)
{
    return (strncmp(dest, src, len));
}

/*!
 *
 * env_mb - implementation
 *
 */
void env_mb(void)
{
    dsb();
}

/*!
 * env_rmb - implementation
 */
void env_rmb(void)
{
    dsb();
}

/*!
 * env_wmb - implementation
 */
void env_wmb(void)
{
    dsb();
}

/*!
 * env_map_vatopa - implementation
 *
 * @param address
 */
uint32_t env_map_vatopa(void *env, void *address)
{
    /* Wrapper maps VA to PA */
    return ((uint32_t)address);
}

/*!
 * env_map_patova - implementation
 *
 * @param address
 */
void *env_map_patova(void *env, uint32_t address)
{
    /* Wrapper maps VA to PA */
    return ((void *)address);
}

/*!
 * env_create_mutex
 *
 * Creates a mutex with the given initial count.
 *
 */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
int32_t env_create_mutex(void **lock, int32_t count, void *context)
#else
int32_t env_create_mutex(void **lock, int32_t count)
#endif
{
    if (count > RL_ENV_MAX_MUTEX_COUNT)
    {
        return -1;
    }

#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
    *lock = context;
#else
    *lock = env_allocate_memory(sizeof(pthread_mutex_t));
#endif
    if (*lock == ((void *)0))
    {
        return -1;
    }
    if (EOK == pthread_mutex_init((pthread_mutex_t *)*lock, ((void *)0)))
    {
        return 0;
    }
    else
    {
#if !(defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1))
        env_free_memory(*lock);
#endif
        return -1;
    }
}

/*!
 * env_delete_mutex
 *
 * Deletes the given lock
 *
 */
void env_delete_mutex(void *lock)
{
    pthread_mutex_destroy(lock);
#if !(defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1))
    env_free_memory(lock);
#endif
}

/*!
 * env_lock_mutex
 *
 * Tries to acquire the lock, if lock is not available then call to
 * this function will suspend.
 */
void env_lock_mutex(void *lock)
{
    pthread_mutex_lock(lock);
}

/*!
 * env_unlock_mutex
 *
 * Releases the given lock.
 */
void env_unlock_mutex(void *lock)
{
    pthread_mutex_unlock(lock);
}

/*!
 * env_create_sync_lock
 *
 * Creates a synchronization lock primitive. It is used
 * when signal has to be sent from the interrupt context to main
 * thread context.
 */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
int32_t env_create_sync_lock(void **lock, int32_t state, void *context)
{
    return env_create_mutex(lock, state, context); /* state=1 .. initially free */
}
#else
int32_t env_create_sync_lock(void **lock, int32_t state)
{
    return env_create_mutex(lock, state); /* state=1 .. initially free */
}
#endif

/*!
 * env_delete_sync_lock
 *
 * Deletes the given lock
 *
 */
void env_delete_sync_lock(void *lock)
{
    if (lock != ((void *)0))
    {
        env_delete_mutex(lock);
    }
}

/*!
 * env_acquire_sync_lock
 *
 * Tries to acquire the lock, if lock is not available then call to
 * this function waits for lock to become available.
 */
void env_acquire_sync_lock(void *lock)
{
    env_lock_mutex(lock);
}

/*!
 * env_release_sync_lock
 *
 * Releases the given lock.
 */
void env_release_sync_lock(void *lock)
{
    env_unlock_mutex(lock);
}

/*!
 * env_sleep_msec
 *
 * Suspends the calling thread for given time , in msecs.
 */
void env_sleep_msec(uint32_t num_msec)
{
    delay(num_msec);
}

/*!
 * env_register_isr
 *
 * Registers interrupt handler data for the given interrupt vector.
 *
 * @param vector_id - virtual interrupt vector number
 * @param data      - interrupt handler data (virtqueue)
 */
void env_register_isr(void *env, uint32_t vector_id, void *data)
{
    env_context_t *ctx = env;

    RL_ASSERT(vector_id < ISR_COUNT);
    if (vector_id < ISR_COUNT)
    {
        ctx->isr_table[vector_id].data = data;
    }
}

/*!
 * env_unregister_isr
 *
 * Unregisters interrupt handler data for the given interrupt vector.
 *
 * @param vector_id - virtual interrupt vector number
 */
void env_unregister_isr(void *env, uint32_t vector_id)
{
    env_context_t *ctx = env;

    RL_ASSERT(vector_id < ISR_COUNT);
    if (vector_id < ISR_COUNT)
    {
        ctx->isr_table[vector_id].data    = ((void *)0);
        ctx->isr_table[vector_id].enabled = 0;
    }
}

/*!
 * env_enable_interrupt
 *
 * Enables the given interrupt
 *
 * @param vector_id   - virtual interrupt vector number
 */
void env_enable_interrupt(void *env, uint32_t vector_id)
{
    env_context_t *ctx = env;

    RL_ASSERT(vector_id < ISR_COUNT);
    if (vector_id < ISR_COUNT)
    {
        ctx->isr_table[vector_id].enabled = 1;
    }
}

/*!
 * env_disable_interrupt
 *
 * Disables the given interrupt
 *
 * @param vector_id   - virtual interrupt vector number
 */
void env_disable_interrupt(void *env, uint32_t vector_id)
{
    env_context_t *ctx = env;

    RL_ASSERT(vector_id < ISR_COUNT);
    if (vector_id < ISR_COUNT)
    {
        ctx->isr_table[vector_id].enabled = 0;
    }
}

/*!
 * env_map_memory
 *
 * Enables memory mapping for given memory region.
 *
 * @param pa   - physical address of memory
 * @param va   - logical address of memory
 * @param size - memory size
 * param flags - flags for cache/uncached  and access type
 */
void env_map_memory(uint32_t pa, uint32_t va, uint32_t size, uint32_t flags)
{
    platform_map_mem_region(va, pa, size, flags);
}

/*!
 * env_disable_cache
 *
 * Disables system caches.
 *
 */
void env_disable_cache(void)
{
    platform_cache_all_flush_invalidate();
    platform_cache_disable();
}

void env_cache_flush(void *data, uint32_t len)
{
#if defined(RL_USE_DCACHE) && (RL_USE_DCACHE == 1)
    platform_cache_flush(data, len);
#endif
}

void env_cache_invalidate(void *data, uint32_t len)
{
#if defined(RL_USE_DCACHE) && (RL_USE_DCACHE == 1)
    platform_cache_invalidate(data, len);
#endif
}

/*!
 *
 * env_get_timestamp
 *
 * Returns a 64 bit time stamp.
 *
 *
 */
uint64_t env_get_timestamp(void)
{
    fprintf(stderr, "%s unsupported\n", __FUNCTION__);
    return 0;
}

/*========================================================= */
/* Util data / functions  */

/**
 * Called from receive thread
 *
 * @param env    Pointer to env context
 * @param vector Vector ID.
 */
void env_isr(void *env, uint32_t vector)
{
    struct isr_info *info;
    env_context_t *ctx = env;

    RL_ASSERT(vector < ISR_COUNT);
    if (vector < ISR_COUNT)
    {
        info = &ctx->isr_table[vector];
        if (info->enabled)
        {
            virtqueue_notification((struct virtqueue *)info->data);
        }
    }
}

/**
 * Called by rpmsg to init an interrupt
 *
 * @param env      Pointer to env context.
 * @param vq_id    Virt. queue ID.
 * @param isr_data Pointer to interrupt data.
 *
 * @return         Execution status.
 */
int32_t env_init_interrupt(void *env, int32_t vq_id, void *isr_data)
{
    env_register_isr(env, vq_id, isr_data);
    return 0;
}

/**
 * Called by rpmsg to deinit an interrupt.
 *
 * @param env   Pointer to env context.
 * @param vq_id Virt. queue ID.
 *
 * @return      Execution status.
 */
int32_t env_deinit_interrupt(void *env, int32_t vq_id)
{
    env_unregister_isr(env, vq_id);
    return 0;
}

/**
 * env_create_queue
 *
 * Creates a message queue.
 *
 * @param queue -  pointer to created queue
 * @param length -  maximum number of elements in the queue
 * @param element_size - queue element size in bytes
 * @param queue_static_storage - pointer to queue static storage buffer
 * @param queue_static_context - pointer to queue static context
 *
 * @return - status of function execution
 */
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
int32_t env_create_queue(void **queue,
                         int32_t length,
                         int32_t element_size,
                         uint8_t *queue_static_storage,
                         rpmsg_static_queue_ctxt *queue_static_context)
#else
int32_t env_create_queue(void **queue, int32_t length, int32_t element_size)
#endif
{
    char name[100];
    struct mq_attr mqstat;

    if (length < 0 || element_size < 0)
    {
        /* Length and size should not be negative */
        *queue = NULL;
        return -1;
    }

#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
    env_queue_t *q = (env_queue_t *)queue_static_context;
#else
    env_queue_t *q = env_allocate_memory(sizeof(env_queue_t));
#endif
    if (q == ((void *)0))
    {
        return -1;
    }
    /* Creates a unique queue in /dev/mq/PID_virtaddr_length */
    sprintf(name, "/%u_0x%lx_%d", getpid(), (uint64_t)q, length);
    mqstat.mq_maxmsg   = length;
    mqstat.mq_msgsize  = element_size;
    mqstat.mq_flags    = 0;
    mqstat.mq_curmsgs  = 0;
    mqstat.mq_recvwait = 0;
    mqstat.mq_sendwait = 0;
    q->msg_len         = element_size;
    q->mqd             = mq_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, &mqstat);
    if (q->mqd == -1)
    {
#if !(defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1))
        env_free_memory(q);
#endif
        fprintf(stderr, "mq_open failed: %s\n", strerror(errno));
        return -1;
    }
    /* Return queue */
    *queue = q;
    return 0;
}

/*!
 * env_delete_queue
 *
 * Deletes the message queue.
 *
 * @param queue - queue to delete
 */
void env_delete_queue(void *queue)
{
    env_queue_t *q = queue;

    mq_close(q->mqd);
#if !(defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1))
    env_free_memory(queue);
#endif
}

/*!
 * env_put_queue
 *
 * Put an element in a queue.
 *
 * @param queue - queue to put element in
 * @param msg - pointer to the message to be put into the queue
 * @param timeout_ms - timeout in ms
 *
 * @return - status of function execution
 */
int32_t env_put_queue(void *queue, void *msg, uintptr_t timeout_ms)
{
    env_queue_t *q = queue;

    if (mq_send(q->mqd, (const char *)msg, q->msg_len, 0))
    {
        fprintf(stderr, "mq_send failed: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

/*!
 * env_get_queue
 *
 * Get an element out of a queue.
 *
 * @param queue - queue to get element from
 * @param msg - pointer to a memory to save the message
 * @param timeout_ms - timeout in ms
 *
 * @return - status of function execution
 */
int32_t env_get_queue(void *queue, void *msg, uintptr_t timeout_ms)
{
    env_queue_t *q = queue;
    if (mq_receive(q->mqd, msg, q->msg_len, ((void *)0)) == -1)
    {
        fprintf(stderr, "mq_receive failed: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

/*!
 * env_get_current_queue_size
 *
 * Get current queue size.
 *
 * @param queue - queue pointer
 *
 * @return - Number of queued items in the queue
 */
int32_t env_get_current_queue_size(void *queue)
{
    struct mq_attr mqstat;
    env_queue_t *q = queue;
    if (mq_getattr(q->mqd, &mqstat) != -1)
    {
        return mqstat.mq_curmsgs;
    }
    return 0;
}
