/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/ipm.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/init.h>

#include "common.h"
#include "rpmsg_lite.h"

#define REMOTE_EPT_ADDR               (30U)
#define LOCAL_EPT_ADDR                (40U)
#define APP_RPMSG_READY_EVENT_DATA    (1U)
#define APP_RPMSG_EP_READY_EVENT_DATA (2U)

#define SHM_MEM_ADDR DT_REG_ADDR(DT_CHOSEN(zephyr_ipc_shm))
#define SHM_MEM_SIZE DT_REG_SIZE(DT_CHOSEN(zephyr_ipc_shm))

#define APP_THREAD_STACK_SIZE (1024)
K_THREAD_STACK_DEFINE(thread_stack, APP_THREAD_STACK_SIZE);
static struct k_thread thread_data;

struct rpmsg_lite_instance *gp_rpmsg_dev_inst;
struct rpmsg_lite_endpoint *gp_rpmsg_ept;
struct rpmsg_lite_instance g_rpmsg_ctxt;
struct rpmsg_lite_ept_static_context g_ept_context;
volatile int32_t g_has_received;

uint32_t *shared_memory = (uint32_t *)SHM_MEM_ADDR;

K_EVENT_DEFINE(wait_event);

typedef struct the_message
{
    uint32_t DATA;
} THE_MESSAGE, *THE_MESSAGE_PTR;

static THE_MESSAGE volatile g_msg = {0};

/* This is the read callback, note we are in a task context when this callback
is invoked, so kernel primitives can be used freely */
static int32_t rpmsg_ept_read_cb(void *payload, uint32_t payload_len, uint32_t src, void *priv)
{
    int32_t *has_received = priv;

    if (payload_len <= sizeof(THE_MESSAGE))
    {
        (void)memcpy((void *)&g_msg, payload, payload_len);
        *has_received = 1;
    }
    (void)printk("Primary core received a msg\r\n");
    (void)printk("Message: Size=%x, DATA = %i\r\n", payload_len, g_msg.DATA);
    return RL_RELEASE;
}

static void application_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    gp_rpmsg_dev_inst =
        rpmsg_lite_master_init(shared_memory, SHM_MEM_SIZE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &g_rpmsg_ctxt);

    gp_rpmsg_ept = rpmsg_lite_create_ept(gp_rpmsg_dev_inst, LOCAL_EPT_ADDR, rpmsg_ept_read_cb, (void *)&g_has_received,
                                         &g_ept_context);

    /* Temporary wait a bit here until remote core will process initialization and is ready to receive data */
    (void)k_event_wait_all(&wait_event, 5, false, K_MSEC(20));

    /* Send the first message to the remoteproc */
    g_msg.DATA = 0U;
    (void)rpmsg_lite_send(gp_rpmsg_dev_inst, gp_rpmsg_ept, REMOTE_EPT_ADDR, (char *)&g_msg, sizeof(THE_MESSAGE),
                          RL_DONT_BLOCK);

    while (g_msg.DATA <= 100U)
    {
        if (1 == g_has_received)
        {
            g_has_received = 0;
            g_msg.DATA++;
            (void)rpmsg_lite_send(gp_rpmsg_dev_inst, gp_rpmsg_ept, REMOTE_EPT_ADDR, (char *)&g_msg, sizeof(THE_MESSAGE),
                                  RL_DONT_BLOCK);
        }
    }

    (void)rpmsg_lite_destroy_ept(gp_rpmsg_dev_inst, gp_rpmsg_ept);
    gp_rpmsg_ept = ((void *)0);
    (void)rpmsg_lite_deinit(gp_rpmsg_dev_inst);

    /* Print the ending banner */
    (void)printk("\r\nRPMsg demo ends\r\n");
}

int main(void)
{
    printk("Starting application thread on Main Core!\n");
    k_thread_create(&thread_data, thread_stack, APP_THREAD_STACK_SIZE, application_thread, NULL, NULL, NULL,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);

    return 0;
}
