/*
 * Copyright 2016-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rpmsg_lite.h"
#include <stdint.h>
#include "pingpong_common.h"
#include "unity.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#include "assert.h"
#include "app.h"
#if defined(SDK_OS_FREE_RTOS)
#include "FreeRTOS.h"
#include "task.h"
#elif defined(FSL_RTOS_XOS)
#include <xtensa/xos.h>
#elif defined(FSL_RTOS_THREADX)
#include "tx_api.h"
#endif

#include "fsl_common.h"
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
#include "fsl_memory.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TC_TRANSFER_COUNT 10
#define DATA_LEN 45
#define TC_LOCAL_EPT_ADDR (30)
#define TC_REMOTE_EPT_ADDR (40)

#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
#define SH_MEM_TOTAL_SIZE (6144)
#if defined(__ICCARM__) /* IAR Workbench */
#pragma location = "rpmsg_sh_mem_section"
char rpmsg_lite_base[SH_MEM_TOTAL_SIZE];
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION) /* Keil MDK */
char rpmsg_lite_base[SH_MEM_TOTAL_SIZE] __attribute__((section("rpmsg_sh_mem_section")));
#elif defined(__GNUC__) /* LPCXpresso */
char rpmsg_lite_base[SH_MEM_TOTAL_SIZE] __attribute__((section(".noinit.$rpmsg_sh_mem")));
#else
#error "RPMsg: Please provide your definition of rpmsg_lite_base[]!"
#endif
#endif /*SH_MEM_NOT_TAKEN_FROM_LINKER */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
struct rpmsg_lite_endpoint *volatile ctrl_ept = NULL;
struct rpmsg_lite_ept_static_context ctrl_ept_ctxt = {0};

rpmsg_queue_handle ctrl_q = NULL;
rpmsg_static_queue_ctxt ctrl_queue_ctxt = {0};
uint8_t ctrl_queue_storage[RL_ENV_QUEUE_STATIC_STORAGE_SIZE] = {0};
struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
struct rpmsg_lite_instance rpmsg_ctxt = {0};

struct rpmsg_lite_endpoint *endpoints[TC_EPT_COUNT] = {NULL};
struct rpmsg_lite_ept_static_context endpoints_ctxt[TC_EPT_COUNT] = {0};
rpmsg_queue_handle qs[TC_EPT_COUNT] = {NULL};
rpmsg_static_queue_ctxt qs_ctxt[TC_EPT_COUNT] = {0};
uint8_t qs_storage[TC_EPT_COUNT][RL_ENV_QUEUE_STATIC_STORAGE_SIZE] = {0};
int32_t ept_num = 0;
rpmsg_ns_handle ns_handle = NULL;
rpmsg_ns_static_context my_ns_ctxt = {0};

static void app_nameservice_isr_cb(uint32_t new_ept, const char *new_ept_name, uint32_t flags, void *user_data)
{
}

// utility: initialize rpmsg and environment
// and wait for default channel
int32_t ts_init_rpmsg(void)
{
    int32_t result = 0;

    env_init();
#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
    // Correct call
    my_rpmsg = rpmsg_lite_remote_init(rpmsg_lite_base, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#else
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
    my_rpmsg = rpmsg_lite_remote_init((void *)MEMORY_ConvertMemoryMapAddress((uint32_t)RPMSG_LITE_SHMEM_BASE, kMEMORY_DMA2Local), RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#else
    // Correct call
    my_rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#endif
#endif /* SH_MEM_NOT_TAKEN_FROM_LINKER */
    TEST_ASSERT_MESSAGE(NULL != my_rpmsg, "init function failed");

    rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);
    ctrl_q = rpmsg_queue_create(my_rpmsg, &ctrl_queue_storage[0], &ctrl_queue_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ctrl_q, "'rpmsg_queue_create' failed");
    ctrl_ept = rpmsg_lite_create_ept(my_rpmsg, TC_LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, ctrl_q, &ctrl_ept_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ctrl_ept, "'rpmsg_lite_create_ept' failed");

    // invalid params for rpmsg_ns_bind
    ns_handle = rpmsg_ns_bind(my_rpmsg, RL_NULL, ((void *)0), &my_ns_ctxt);
    TEST_ASSERT_MESSAGE(RL_NULL == ns_handle, "'rpmsg_ns_bind' with bad app_cb param failed");
    
    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, ((void *)0), &my_ns_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ns_handle, "'rpmsg_ns_bind' failed");
    
    // invalid params for rpmsg_ns_announce
    result = rpmsg_ns_announce(my_rpmsg, ctrl_ept, RL_NULL, (uint32_t)RL_NS_CREATE);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_ns_announce' with bad ept_name param failed");
    result = rpmsg_ns_announce(my_rpmsg, RL_NULL, RPMSG_LITE_NS_ANNOUNCE_STRING, (uint32_t)RL_NS_CREATE);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_ns_announce' with bad new_ept param failed");

    // send invalid NS message to the RL_NS_EPT_ADDR - wrong paylod_len identified in the ns cb and the message is dropped (condition coverage increase)
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    rpmsg_lite_send(my_rpmsg, ctrl_ept, RL_NS_EPT_ADDR, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE), RL_BLOCK);
    
    /* wait for a while to allow the primary side to bind_ns and register the NS callback */
    env_sleep_msec(200);
    // send correct NS message
    result = rpmsg_ns_announce(my_rpmsg, ctrl_ept, RPMSG_LITE_NS_ANNOUNCE_STRING, (uint32_t)RL_NS_CREATE);
    TEST_ASSERT_MESSAGE(RL_SUCCESS == result, "'rpmsg_ns_announce' failed");
    return 0;
}

// utility: deinitialize rpmsg and enviroment
int32_t ts_deinit_rpmsg(void)
{
    rpmsg_lite_destroy_ept(my_rpmsg, ctrl_ept);
    rpmsg_queue_destroy(my_rpmsg, ctrl_q);
    rpmsg_ns_unbind(my_rpmsg, ns_handle);
    ns_handle = NULL;
    env_memset(&my_ns_ctxt, 0, sizeof(rpmsg_ns_static_context));
    rpmsg_lite_deinit(my_rpmsg);
    my_rpmsg = NULL;
    env_memset(&rpmsg_ctxt, 0, sizeof(struct rpmsg_lite_instance));
    return 0;
}

int32_t pattern_cmp(char *buffer, char pattern, int32_t len)
{
    for (int32_t i = 0; i < len; i++)
        if (buffer[i] != pattern)
            return -1;
    return 0;
}

/******************************************************************************
 * Responder task
 *****************************************************************************/
void responder_task(void)
{
    int32_t ret_value = 0;
    void *data_addr = NULL;
    uint32_t num_of_received_control_bytes;
    uint32_t i = 0;
    uint32_t src = 0;
    struct rpmsg_lite_endpoint *my_ept = NULL;
    rpmsg_queue_handle q = NULL;
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM data_destroy_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    unsigned char *recv_buffer[BUFFER_MAX_LENGTH];
    void *nocopy_buffer_ptr = NULL; // pointer to receive data in no-copy mode
    uint32_t buf_size = 0;     /* use to store size of buffer for
                                       rpmsg_rtos_alloc_tx_buffer() */

    ret_value = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == ret_value, "Testing function init rpmsg");
    if (ret_value)
        goto end;

    while (1)
    {
        ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                     &num_of_received_control_bytes, RL_BLOCK);

        if (0 != ret_value)
        {
            /* PRINTF("Responder task receive error: %i\n", ret_value); */
        }
        else
        {
            /* PRINTF("Responder task received a msg\n\r"); */
            /* PRINTF("Message: Size=0x%x, CMD = 0x%x, DATA = 0x%x 0x%x 0x%x 0x%x\n\r", num_of_received_control_bytes,
                   msg.CMD, msg.DATA[0], msg.DATA[1], msg.DATA[2], msg.DATA[3]); */

            switch (msg.CMD)
            {
                case CTR_CMD_CREATE_EP:
                    ret_value = -1;
                    env_memcpy((void *)(&data_create_ept_param), (void *)msg.DATA,
                               sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));

                    q = rpmsg_queue_create(my_rpmsg, &qs_storage[ept_num][0], &qs_ctxt[ept_num]);
                    my_ept =
                        rpmsg_lite_create_ept(my_rpmsg, data_create_ept_param.ept_to_create_addr, rpmsg_queue_rx_cb, q, &endpoints_ctxt[ept_num]);

                    if ((NULL == my_ept) || (NULL == q))
                    {
                        if (NULL != q)
                        {
                            rpmsg_queue_destroy(my_rpmsg, q);
                        }

                        if (NULL != my_ept)
                        {
                            rpmsg_lite_destroy_ept(my_rpmsg, my_ept);
                        }

                        ret_value = 1; /* Fail to create enpoint */
                    }
                    else
                    {
                        qs[ept_num] = q;
                        endpoints[ept_num++] = my_ept;
                        ret_value = 0;
                    }

                    if (ACK_REQUIRED_YES == msg.ACK_REQUIRED)
                    {
                        ack_msg.CMD_ACK = CTR_CMD_CREATE_EP;
                        ack_msg.RETURN_VALUE = ret_value;
                        env_memcpy((void *)ack_msg.RESP_DATA, (void *)&(my_ept->addr), sizeof(uint32_t));
                        /* Send ack_msg to sender */
                        ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_create_ept_param.ept_to_ack_addr,
                                                    (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE), RL_BLOCK);
                    }
                    break;

                case CTR_CMD_DESTROY_EP:
                    ret_value = -1;
                    env_memcpy((void *)(&data_destroy_ept_param), (void *)msg.DATA,
                               sizeof(CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM));

                    if (data_destroy_ept_param.ept_to_destroy_addr == DESTROY_ALL_EPT)
                    {
                        for (i = 0; i < ept_num; i++)
                        {
                            rpmsg_lite_destroy_ept(my_rpmsg, endpoints[i]);
                            rpmsg_queue_destroy(my_rpmsg, qs[i]);
                        }
                        ept_num = 0;
                        /* We can't check return value of destroy function due to this function return void */
                        ret_value = 0;
                    }
                    else
                    {
                        rpmsg_lite_destroy_ept(my_rpmsg, endpoints[data_destroy_ept_param.ept_to_destroy_addr]);
                        rpmsg_queue_destroy(my_rpmsg, qs[data_destroy_ept_param.ept_to_destroy_addr]);
                        /* We can't check return value of destroy function due to this function return void */
                        ret_value = 0;
                    }

                    if (ACK_REQUIRED_YES == msg.ACK_REQUIRED)
                    {
                        ack_msg.CMD_ACK = CTR_CMD_DESTROY_EP;
                        ack_msg.RETURN_VALUE = ret_value;
                        /* Send ack_msg to tea_control_endpoint */
                        ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_destroy_ept_param.ept_to_ack_addr,
                                                    (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE), RL_BLOCK);
                    }
                    break;
                case CTR_CMD_RECV:
                    ret_value = -1;
                    env_memcpy((void *)&data_recv_param, (void *)msg.DATA,
                               (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));

                    my_ept = NULL;
                    q = NULL;
                    for (i = 0; i < ept_num; i++)
                    {
                        if (endpoints[i]->addr == data_recv_param.responder_ept_addr)
                        {
                            my_ept = endpoints[i];
                            q = qs[i];
                            break;
                        }
                    }

                    if (CMD_RECV_MODE_COPY == data_recv_param.mode)
                    {
                        if (RL_BLOCK == data_recv_param.timeout_ms)
                        {
                            ret_value =
                                rpmsg_queue_recv(my_rpmsg, q, &src, (char *)recv_buffer, data_recv_param.buffer_size,
                                                 &num_of_received_control_bytes, RL_BLOCK);
                        }
                        else if (RL_DONT_BLOCK == data_recv_param.timeout_ms)
                        {
                            /* receive function with non-blocking call */
                            do
                            {
                                ret_value = rpmsg_queue_recv(
                                    my_rpmsg, q, &src, (char *)recv_buffer, data_recv_param.buffer_size,
                                    &num_of_received_control_bytes, RL_DONT_BLOCK);
                                if (ret_value == RL_ERR_PARAM)
                                    break;
                            } while (0 != ret_value);
                        }
                        else
                        {
#if defined(SDK_OS_FREE_RTOS)
                            TickType_t tick_count = xTaskGetTickCount();
#elif defined(FSL_RTOS_XOS)
                            uint64_t tick_count = xos_get_system_cycles();
#elif defined(FSL_RTOS_THREADX)
                            uint64_t tick_count = tx_time_get();
#endif
                            ret_value =
                                rpmsg_queue_recv(my_rpmsg, q, &src, (char *)recv_buffer, data_recv_param.buffer_size,
                                                 &num_of_received_control_bytes, data_recv_param.timeout_ms);
#if defined(SDK_OS_FREE_RTOS)
                            tick_count = xTaskGetTickCount() - tick_count;
                            // Calculate milisecond
                            ack_msg.TIMEOUT_MSEC = tick_count * (1000 / configTICK_RATE_HZ);
#elif defined(FSL_RTOS_XOS)
                            tick_count = xos_get_system_cycles() - tick_count;
                            // Calculate milisecond
                            ack_msg.TIMEOUT_MSEC = xos_cycles_to_msecs(tick_count);
#elif defined(FSL_RTOS_THREADX)
                            tick_count = tx_time_get() - tick_count;
                            // Calculate millisecond
                            ack_msg.TIMEOUT_MSEC = tick_count * (1000 / TX_TIMER_TICKS_PER_SECOND);
#endif
                        }
                    }
                    else if (CMD_RECV_MODE_NOCOPY == data_recv_param.mode)
                    {
                        if (RL_BLOCK == data_recv_param.timeout_ms)
                        {
                            ret_value =
                                rpmsg_queue_recv_nocopy(my_rpmsg, q, &src, (char **)&nocopy_buffer_ptr,
                                                        &num_of_received_control_bytes, RL_BLOCK);
                        }
                        else if (RL_DONT_BLOCK == data_recv_param.timeout_ms)
                        {
                            /* receive function with non-blocking call */
                            do
                            {
                                ret_value =
                                    rpmsg_queue_recv_nocopy(my_rpmsg, q, &src, (char **)&nocopy_buffer_ptr,
                                                            &num_of_received_control_bytes, RL_DONT_BLOCK);

                                if (ret_value == RL_ERR_PARAM)
                                    break;
                            } while (0 != ret_value);
                        }
                        else
                        {
#if defined(SDK_OS_FREE_RTOS)
                            TickType_t tick_count = xTaskGetTickCount();
#elif defined(FSL_RTOS_XOS)
                            uint64_t tick_count = xos_get_system_cycles();
#elif defined(FSL_RTOS_THREADX)
                            uint64_t tick_count = tx_time_get();
#endif
                            ret_value =
                                rpmsg_queue_recv_nocopy(my_rpmsg, q, &src, (char **)&nocopy_buffer_ptr,
                                                        &num_of_received_control_bytes, data_recv_param.timeout_ms);
#if defined(SDK_OS_FREE_RTOS)
                            tick_count = xTaskGetTickCount() - tick_count;
                            // Calculate milisecond
                            ack_msg.TIMEOUT_MSEC = tick_count * (1000 / configTICK_RATE_HZ);
#elif defined(FSL_RTOS_XOS)
                            tick_count = xos_get_system_cycles() - tick_count;
                            // Calculate milisecond
                            ack_msg.TIMEOUT_MSEC = xos_cycles_to_msecs(tick_count);
#elif defined(FSL_RTOS_THREADX)
                            tick_count = tx_time_get() - tick_count;
                            // Calculate millisecond
                            ack_msg.TIMEOUT_MSEC = tick_count * (1000 / TX_TIMER_TICKS_PER_SECOND);
#endif
                        }

                        /* Free buffer when use rpmsg_rtos_recv_nocopy function */
                        if (0 == ret_value)
                        {
                            env_memcpy((void *)recv_buffer, (void *)nocopy_buffer_ptr, num_of_received_control_bytes);
                            ret_value = rpmsg_queue_nocopy_free(my_rpmsg, nocopy_buffer_ptr);
                        }
                    }

                    if (ACK_REQUIRED_YES == msg.ACK_REQUIRED)
                    {
                        ack_msg.CMD_ACK = CTR_CMD_RECV;
                        ack_msg.RETURN_VALUE = ret_value;

                        env_memcpy((void *)ack_msg.RESP_DATA, (void *)recv_buffer, num_of_received_control_bytes);
                        env_memset(recv_buffer, 0, BUFFER_MAX_LENGTH);
                        /* Send ack_msg to sender */
                        ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_recv_param.ept_to_ack_addr,
                                                    (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE), RL_BLOCK);
                    }

                    break;
                case CTR_CMD_SEND:
                    ret_value = -1;
                    env_memcpy((void *)&data_send_param, (void *)msg.DATA,
                               (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));

                    if (CMD_SEND_MODE_COPY == data_send_param.mode)
                    {
                        for (i = 0; i < data_send_param.repeat_count; i++)
                        {
                            ret_value =
                                rpmsg_lite_send(my_rpmsg, ctrl_ept, data_send_param.dest_addr,
                                                (char *)data_send_param.msg, data_send_param.msg_size, RL_BLOCK);
                        }
                    }
                    else if (CMD_SEND_MODE_NOCOPY == data_send_param.mode)
                    {
                        for (i = 0; i < data_send_param.repeat_count; i++)
                        {
                            data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_BLOCK);
                            if (buf_size == 0 || data_addr == NULL)
                            {
                                ret_value = -1; /* Failed to alloc tx buffer */
                                break;
                            }
                            env_memcpy((void *)data_addr, (void *)data_send_param.msg, data_send_param.msg_size);
                            ret_value = rpmsg_lite_send_nocopy(my_rpmsg, ctrl_ept, data_send_param.dest_addr,
                                                               (char *)data_addr, data_send_param.msg_size);
                        }
                    }

                    if (ACK_REQUIRED_YES == msg.ACK_REQUIRED)
                    {
                        ack_msg.CMD_ACK = CTR_CMD_SEND;
                        ack_msg.RETURN_VALUE = ret_value;
                        /* Send ack_msg to tea_control_endpoint */
                        ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_send_param.ept_to_ack_addr,
                                                    (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE), RL_BLOCK);
                    }

                    break;
                case CTR_CMD_FINISH:
                    goto end;
                    break;
            }
        }
    }

end:
    ret_value = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == ret_value, "negative number");
}

void run_tests(void *unused)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("04_ping_pong_rtos_sec_core");
    __coveragescanner_install("04_ping_pong_rtos_sec_core.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(responder_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
