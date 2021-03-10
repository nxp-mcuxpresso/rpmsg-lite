/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**************************************************************************
 * FILE NAME
 *
 *       rpmsg_env_threadx.c
 *
 *
 * DESCRIPTION
 *
 *       This file is ThreadX Implementation of env layer for OpenAMP.
 *
 *
 **************************************************************************/
#include "rpmsg_env.h"
#include "tx_api.h"
#include "tx_event_flags.h"
#include "rpmsg_platform.h"
#include "fsl_common.h"
#include "rpmsg_compiler.h"
#include "fsl_component_mem_manager.h"
#include <stdlib.h>
#include <string.h>
#include "virtqueue.h"

static int32_t env_init_counter = 0;
static TX_SEMAPHORE env_sema;

/* RL_ENV_MAX_MUTEX_COUNT is an arbitrary count greater than 'count'
   if the inital count is 1, this function behaves as a mutex
   if it is greater than 1, it acts as a "resource allocator" with
   the maximum of 'count' resources available.
   Currently, only the first use-case is applicable/applied in RPMsg-Lite.
 */
#define RL_ENV_MAX_MUTEX_COUNT (10)

/* Max supported ISR counts */
#define ISR_COUNT (32U)
/*!
 * Structure to keep track of registered ISR's.
 */
struct isr_info
{
    void *data;
};
static struct isr_info isr_table[ISR_COUNT];

#if defined(RL_USE_ENVIRONMENT_CONTEXT) && (RL_USE_ENVIRONMENT_CONTEXT == 1)
#error "This RPMsg-Lite port requires RL_USE_ENVIRONMENT_CONTEXT set to 0"
#endif

/*!
 * env_in_isr
 *
 * @returns - true, if currently in ISR
 *
 */
static int32_t env_in_isr(void)
{
    return platform_in_isr();
}

/*!
 * env_init
 *
 * Initializes OS/ThreadX environment.
 *
 */
int32_t env_init(void)
{
    int32_t retval;
    uint32_t regPrimask = DisableGlobalIRQ(); /* stop scheduler */

    // verify 'env_init_counter'
    RL_ASSERT(env_init_counter >= 0);
    if (env_init_counter < 0)
    {
        EnableGlobalIRQ(regPrimask); /* re-enable scheduler */
        return -1;
    }
    env_init_counter++;
    // multiple call of 'env_init' - return ok
    if (env_init_counter == 1)
    {
        // first call
        if (TX_SUCCESS != _tx_semaphore_create((TX_SEMAPHORE *)&env_sema, NULL, 0))
        {
            EnableGlobalIRQ(regPrimask);
            return -1;
        }
        (void)memset(isr_table, 0, sizeof(isr_table));
        EnableGlobalIRQ(regPrimask);
        retval = platform_init();
        tx_semaphore_put((TX_SEMAPHORE *)&env_sema);

        return retval;
    }
    else
    {
        EnableGlobalIRQ(regPrimask);
        /* Get the semaphore and then return it,
         * this allows for platform_init() to block
         * if needed and other tasks to wait for the
         * blocking to be done.
         * This is in ENV layer as this is ENV specific.*/
        if (TX_SUCCESS == tx_semaphore_get((TX_SEMAPHORE *)&env_sema, TX_WAIT_FOREVER))
        {
            tx_semaphore_put((TX_SEMAPHORE *)&env_sema);
        }
        return 0;
    }
}

/*!
 * env_deinit
 *
 * Uninitializes OS/BM environment.
 *
 * @returns - execution status
 */
int32_t env_deinit(void)
{
    int32_t retval;

    uint32_t regPrimask = DisableGlobalIRQ(); /* stop scheduler */
    // verify 'env_init_counter'
    RL_ASSERT(env_init_counter > 0);
    if (env_init_counter <= 0)
    {
        EnableGlobalIRQ(regPrimask);
        return -1;
    }

    // counter on zero - call platform deinit
    env_init_counter--;
    // multiple call of 'env_deinit' - return ok
    if (env_init_counter <= 0)
    {
        // last call
        (void)memset(isr_table, 0, sizeof(isr_table));
        retval = platform_deinit();
        (void)_tx_semaphore_delete((TX_SEMAPHORE *)&env_sema);
        (void)memset(&env_sema, 0, sizeof(env_sema));
        EnableGlobalIRQ(regPrimask);
        return retval;
    }
    else
    {
        EnableGlobalIRQ(regPrimask);
        return 0;
    }
}

/*!
 * env_allocate_memory - implementation
 *
 * @param size
 */
void *env_allocate_memory(uint32_t size)
{
    return (MEM_BufferAlloc(size));
}

/*!
 * env_free_memory - implementation
 *
 * @param ptr
 */
void env_free_memory(void *ptr)
{
    if (ptr != ((void *)0))
    {
        MEM_BufferFree(ptr);
    }
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
    (void)memset(ptr, value, size);
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
    MEM_BARRIER();
}

/*!
 * env_rmb - implementation
 */
void env_rmb(void)
{
    MEM_BARRIER();
}

/*!
 * env_wmb - implementation
 */
void env_wmb(void)
{
    MEM_BARRIER();
}

/*!
 * env_map_vatopa - implementation
 *
 * @param address
 */
uint32_t env_map_vatopa(void *address)
{
    return platform_vatopa(address);
}

/*!
 * env_map_patova - implementation
 *
 * @param address
 */
void *env_map_patova(uint32_t address)
{
    return platform_patova(address);
}

/*!
 * env_create_mutex
 *
 * Creates a mutex with the given initial count.
 *
 */
int32_t env_create_mutex(void **lock, int32_t count)
{
    TX_SEMAPHORE *semaphore_ptr;

    semaphore_ptr = (TX_SEMAPHORE *)env_allocate_memory(sizeof(TX_SEMAPHORE));
    if (semaphore_ptr == ((void *)0))
    {
        return -1;
    }

    if (count > RL_ENV_MAX_MUTEX_COUNT)
    {
        return -1;
    }

    if (TX_SUCCESS != _tx_semaphore_create((TX_SEMAPHORE *)semaphore_ptr, NULL, count))
    {
        return -1;
    }
    *lock = (void *)semaphore_ptr;
    return 0;
}

/*!
 * env_delete_mutex
 *
 * Deletes the given lock
 *
 */
void env_delete_mutex(void *lock)
{
    (void)_tx_semaphore_delete((TX_SEMAPHORE *)lock);
    env_free_memory(lock);
}

/*!
 * env_lock_mutex
 *
 * Tries to acquire the lock, if lock is not available then call to
 * this function will suspend.
 */
void env_lock_mutex(void *lock)
{
    if (env_in_isr() == 0)
    {
        (void)tx_semaphore_get((TX_SEMAPHORE *)lock, TX_WAIT_FOREVER);
    }
}

/*!
 * env_unlock_mutex
 *
 * Releases the given lock.
 */
void env_unlock_mutex(void *lock)
{
    if (env_in_isr() == 0)
    {
        tx_semaphore_put((TX_SEMAPHORE *)lock);
    }
}

/*!
 * env_create_sync_lock
 *
 * Creates a synchronization lock primitive. It is used
 * when signal has to be sent from the interrupt context to main
 * thread context.
 */
int32_t env_create_sync_lock(void **lock, int32_t state)
{
    return env_create_mutex(lock, state); /* state=1 .. initially free */
}

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
    if (lock != ((void *)0))
    {
        env_lock_mutex(lock);
    }
}

/*!
 * env_release_sync_lock
 *
 * Releases the given lock.
 */
void env_release_sync_lock(void *lock)
{
    if (lock != ((void *)0))
    {
        env_unlock_mutex(lock);
    }
}

/*!
 * env_sleep_msec
 *
 * Suspends the calling thread for given time , in msecs.
 */
void env_sleep_msec(uint32_t num_msec)
{
    tx_thread_sleep(num_msec);
}

/*!
 * env_register_isr
 *
 * Registers interrupt handler data for the given interrupt vector.
 *
 * @param vector_id - virtual interrupt vector number
 * @param data      - interrupt handler data (virtqueue)
 */
void env_register_isr(uint32_t vector_id, void *data)
{
    RL_ASSERT(vector_id < ISR_COUNT);
    if (vector_id < ISR_COUNT)
    {
        isr_table[vector_id].data = data;
    }
}

/*!
 * env_unregister_isr
 *
 * Unregisters interrupt handler data for the given interrupt vector.
 *
 * @param vector_id - virtual interrupt vector number
 */
void env_unregister_isr(uint32_t vector_id)
{
    RL_ASSERT(vector_id < ISR_COUNT);
    if (vector_id < ISR_COUNT)
    {
        isr_table[vector_id].data = ((void *)0);
    }
}

/*!
 * env_enable_interrupt
 *
 * Enables the given interrupt
 *
 * @param vector_id   - virtual interrupt vector number
 */

void env_enable_interrupt(uint32_t vector_id)
{
    (void)platform_interrupt_enable(vector_id);
}

/*!
 * env_disable_interrupt
 *
 * Disables the given interrupt
 *
 * @param vector_id   - virtual interrupt vector number
 */

void env_disable_interrupt(uint32_t vector_id)
{
    (void)platform_interrupt_disable(vector_id);
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

/*========================================================= */
/* Util data / functions  */

void env_isr(uint32_t vector)
{
    struct isr_info *info;
    RL_ASSERT(vector < ISR_COUNT);
    if (vector < ISR_COUNT)
    {
        info = &isr_table[vector];
        virtqueue_notification((struct virtqueue *)info->data);
    }
}

/*
 * env_create_queue
 *
 * Creates a message queue.
 *
 * @param queue -  pointer to created queue
 * @param length -  maximum number of elements in the queue
 * @param element_size - queue element size in bytes
 *
 * @return - status of function execution
 */
int32_t env_create_queue(void **queue, int32_t length, int32_t element_size)
{
    if (TX_SUCCESS == _tx_queue_create((TX_QUEUE *)*queue, NULL, element_size, NULL, length))
    {
        return 0;
    }
    else
    {
        return -1;
    }
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
    tx_queue_delete(queue);
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

int32_t env_put_queue(void *queue, void *msg, uint32_t timeout_ms)
{
    if (TX_SUCCESS == tx_queue_send((TX_QUEUE *)queue, msg, timeout_ms))
    {
        return 0;
    }
    else
    {
        return -1;
    }
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

int32_t env_get_queue(void *queue, void *msg, uint32_t timeout_ms)
{
    if (TX_SUCCESS == tx_queue_receive((TX_QUEUE *)queue, msg, timeout_ms))
    {
        return 0;
    }
    else
    {
        return -1;
    }
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
    int32_t enqueued;
    ULONG available_storage;
    TX_THREAD *first_suspended;
    ULONG suspended_count;
    TX_QUEUE *next_queue;
    if (TX_SUCCESS == tx_queue_info_get((TX_QUEUE *)queue, NULL, (ULONG *)&enqueued, &available_storage,
                                        &first_suspended, &suspended_count, &next_queue))
    {
        return 0;
    }
    else
    {
        return -1;
    }
}
