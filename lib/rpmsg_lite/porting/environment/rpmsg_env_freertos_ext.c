/*
 * Copyright (c) 2014, Mentor Graphics Corporation
 * Copyright (c) 2015 Xilinx, Inc.
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * Copyright 2016-2023 NXP
 * All rights reserved.
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
#ifdef __COVERAGESCANNER__

/**************************************************************************
 * FILE NAME
 *
 *       rpmsg_env_freertos_ext.c
 *
 *
 * DESCRIPTION
 *
 *       This file is FreeRTOS Implementation of env layer for OpenAMP.
 *
 *
 **************************************************************************/

#include "rpmsg_env.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "rpmsg_platform.h"
#include "event_groups.h"

extern EventGroupHandle_t event_group;

/*!
 * env_acquire_sync_lock
 *
 * Tries to acquire the lock, if lock is not available then call to
 * this function waits for lock to become available.
 */
void env_acquire_sync_lock(void *lock)
{
    BaseType_t xTaskWokenByReceive = pdFALSE;
    SemaphoreHandle_t xSemaphore   = (SemaphoreHandle_t)lock;
    if (platform_in_isr() != 0)
    {
        (void)xSemaphoreTakeFromISR(xSemaphore, &xTaskWokenByReceive);
        portEND_SWITCHING_ISR(xTaskWokenByReceive);
    }
    else
    {
        (void)xSemaphoreTake(xSemaphore, portMAX_DELAY);
    }
}

/*!
 * env_release_sync_lock
 *
 * Releases the given lock.
 */
void env_release_sync_lock(void *lock)
{
    BaseType_t xTaskWokenByReceive = pdFALSE;
    SemaphoreHandle_t xSemaphore   = (SemaphoreHandle_t)lock;
    if (platform_in_isr() != 0)
    {
        (void)xSemaphoreGiveFromISR(xSemaphore, &xTaskWokenByReceive);
        portEND_SWITCHING_ISR(xTaskWokenByReceive);
    }
    else
    {
        (void)xSemaphoreGive(xSemaphore);
    }
}

/*!
 * env_tx_callback
 *
 * Set event to notify task waiting in env_wait_for_link_up().
 *
 */
void env_tx_callback(uint32_t link_id)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (platform_in_isr() != 0)
    {
        (void)xEventGroupSetBitsFromISR(event_group, (EventBits_t)(1UL << link_id), &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        (void)xEventGroupSetBits(event_group, (EventBits_t)(1UL << link_id));
    }
}

/*!
 * env_wait_for_link_up
 *
 * Wait until the link_state parameter of the rpmsg_lite_instance is set.
 * Utilize events to avoid busy loop implementation.
 *
 */
uint32_t env_wait_for_link_up(volatile uint32_t *link_state, uint32_t link_id, uint32_t timeout_ms)
{
    (void)xEventGroupClearBits(event_group, (EventBits_t)(1UL << link_id));
    if (*link_state != 1U)
    {
        EventBits_t uxBits;
        uxBits = xEventGroupWaitBits(event_group, (EventBits_t)(1UL << link_id), pdFALSE, pdTRUE,
                                     ((portMAX_DELAY == timeout_ms) ? portMAX_DELAY : timeout_ms / portTICK_PERIOD_MS));
        if (uxBits == (EventBits_t)(1UL << link_id))
        {
            return 1U;
        }
        else
        {
            /* timeout */
            return 0U;
        }
    }
    else
    {
        return 1U;
    }
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
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (platform_in_isr() != 0)
    {
        if (xQueueSendFromISR(queue, msg, &xHigherPriorityTaskWoken) == pdPASS)
        {
            portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
            return 1;
        }
    }
    else
    {
        if (xQueueSend(queue, msg, ((portMAX_DELAY == timeout_ms) ? portMAX_DELAY : timeout_ms / portTICK_PERIOD_MS)) ==
            pdPASS)
        {
            return 1;
        }
    }
    return 0;
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
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (platform_in_isr() != 0)
    {
        if (xQueueReceiveFromISR(queue, msg, &xHigherPriorityTaskWoken) == pdPASS)
        {
            portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
            return 1;
        }
    }
    else
    {
        if (xQueueReceive(queue, msg,
                          ((portMAX_DELAY == timeout_ms) ? portMAX_DELAY : timeout_ms / portTICK_PERIOD_MS)) == pdPASS)
        {
            return 1;
        }
    }
    return 0;
}
#endif /* __COVERAGESCANNER__*/
