/*
 * Copyright 2016-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rpmsg_lite.h"
#include <stdint.h>
#include "pingpong_common.h"
#include "unity.h"
#include "assert.h"
#include "rpmsg_queue.h"
#include "app.h"
#if defined(SDK_OS_FREE_RTOS)
#include "FreeRTOS.h"
#include "task.h"
#elif defined(FSL_RTOS_XOS)
#include <xtensa/xos.h>
#include <stdlib.h>
#elif defined(FSL_RTOS_THREADX)
#include "tx_api.h"
#endif
#include "string.h"
#include "stdint.h"

#include "fsl_common.h"
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
#include "fsl_memory.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TC_TRANSFER_COUNT 10
#define DATA_LEN 45
#define WORKER_NUMBER (2) /* Number of concurrent threads, needs enough heap... */
#define TEST_EPT_NUM_BASE (60)
#define REMOTE_DEFAULT_EPT (TC_REMOTE_EPT_ADDR)
#if defined (CPU_LPC54102J512BD64_cm0plus)
#define TEST_THREAD_SAFETY_TASK_STACK_SIZE (192) /* Verified for Niobe1/2 boards */
#else
#define TEST_THREAD_SAFETY_TASK_STACK_SIZE (300)
#endif

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
typedef struct
{
    struct rpmsg_lite_endpoint *ack_ept;
    struct rpmsg_lite_ept_static_context ack_ept_ctxt;
    rpmsg_queue_handle ack_q;
    rpmsg_static_queue_ctxt ack_q_ctxt;
    uint8_t ack_q_storage[RL_ENV_QUEUE_STATIC_STORAGE_SIZE];
    uint8_t worker_id;
#if defined(SDK_OS_FREE_RTOS)
    TaskHandle_t test_thread_handle;
    StaticTask_t xTaskBuffer;
    void *task_stack_base;
#elif defined(FSL_RTOS_XOS)
    XosThread test_thread_handle;
    void *task_stack_base;
#elif defined(FSL_RTOS_THREADX)
    TX_THREAD test_thread_handle;
    void *task_stack_base;
#endif
} initParamTypedef;

/*******************************************************************************
 * Code
 ******************************************************************************/
volatile int32_t globCntr = 0;

struct rpmsg_lite_endpoint *volatile ctrl_ept = NULL;
struct rpmsg_lite_ept_static_context ctrl_ept_ctxt = {0};

rpmsg_queue_handle ctrl_q = NULL;
rpmsg_static_queue_ctxt ctrl_queue_ctxt = {0};
uint8_t ctrl_queue_storage[RL_ENV_QUEUE_STATIC_STORAGE_SIZE] = {0};
struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
struct rpmsg_lite_instance rpmsg_ctxt = {0};

#if defined(FSL_RTOS_THREADX)
VOID sendRecvTestTask(ULONG arg);
#else
void sendRecvTestTask(void *ept_number);
#endif

// utility: initialize rpmsg and environment
// and wait for default channel
int32_t ts_init_rpmsg(void)
{
    env_init();
#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
    my_rpmsg = rpmsg_lite_remote_init(rpmsg_lite_base, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#else
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
    my_rpmsg = rpmsg_lite_remote_init((void *)MEMORY_ConvertMemoryMapAddress((uint32_t)RPMSG_LITE_SHMEM_BASE, kMEMORY_DMA2Local), RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#else
    my_rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#endif
#endif /* SH_MEM_NOT_TAKEN_FROM_LINKER */
    TEST_ASSERT_MESSAGE(NULL != my_rpmsg, "init function failed");

    ctrl_q = rpmsg_queue_create(my_rpmsg, &ctrl_queue_storage[0], &ctrl_queue_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ctrl_q, "'rpmsg_queue_create' failed");

    ctrl_ept = rpmsg_lite_create_ept(my_rpmsg, TC_LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, ctrl_q, &ctrl_ept_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ctrl_ept, "'rpmsg_lite_create_ept' failed");

    rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);
    return 0;
}

// utility: deinitialize rpmsg and environment
int32_t ts_deinit_rpmsg(void)
{
    rpmsg_lite_destroy_ept(my_rpmsg, ctrl_ept);
    rpmsg_queue_destroy(my_rpmsg, ctrl_q);
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

/*
 * Destroy an endpoint on the other side
 */
int32_t ts_destroy_ept(int32_t addr, struct rpmsg_lite_endpoint *ack_ept, rpmsg_queue_handle ack_q)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    int32_t ret_value;
    uint32_t num_of_received_bytes = 0;
    uint32_t src;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM data_destroy_ept_param;

    data_destroy_ept_param.ept_to_ack_addr = ack_ept->addr;
    data_destroy_ept_param.ept_to_destroy_addr = addr;

    msg.CMD = CTR_CMD_DESTROY_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_destroy_ept_param), sizeof(CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM));
    ret_value = rpmsg_lite_send(my_rpmsg, ack_ept, TC_REMOTE_EPT_ADDR, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 == ret_value, "error! failed to send CTR_CMD_DESTROY_EP command to other side");
    /* Receive respond from other core */
    ret_value = rpmsg_queue_recv(my_rpmsg, ack_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);

    TEST_ASSERT_MESSAGE(0 == ret_value, "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE(CTR_CMD_DESTROY_EP == ack_msg.CMD_ACK,
                        "error! expecting acknowledge of CTR_CMD_DESTROY_EP copmmand");
    TEST_ASSERT_MESSAGE(0 == ack_msg.RETURN_VALUE, "error! failed to destroy endpoints on other side");

    return 0;
}

/*
 * Thread safety testing
 */
void tc_1_main_task(void)
{
    int32_t result, i;
    initParamTypedef *initParam = NULL;
    uint32_t src;
    uint32_t recved;
    char buf[32];
    CONTROL_MESSAGE msg = {0};

    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    if (result == 0)
    {
        rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)buf, 32, &recved, RL_BLOCK);
        for (i = 0; i < WORKER_NUMBER; i++)
        {
#if defined(SDK_OS_FREE_RTOS)
            initParam = malloc(sizeof(initParamTypedef));
#elif defined(FSL_RTOS_XOS) || defined(FSL_RTOS_THREADX)
            initParam = env_allocate_memory(sizeof(initParamTypedef));
#endif
            TEST_ASSERT_MESSAGE(NULL != initParam, "Out of memory");
            initParam->worker_id = (uint8_t)i;
            initParam->ack_q = rpmsg_queue_create(my_rpmsg, &initParam->ack_q_storage[0], &initParam->ack_q_ctxt);
            TEST_ASSERT_MESSAGE(NULL != initParam->ack_q, "Out of memory");
            initParam->ack_ept = rpmsg_lite_create_ept(my_rpmsg, RL_ADDR_ANY, rpmsg_queue_rx_cb, initParam->ack_q, &initParam->ack_ept_ctxt);
            TEST_ASSERT_MESSAGE(NULL != initParam->ack_ept, "Out of memory");
#if defined(SDK_OS_FREE_RTOS)
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
            result = xTaskCreate(sendRecvTestTask, "THREAD_SAFETY_TASK", TEST_THREAD_SAFETY_TASK_STACK_SIZE, (void *)(initParam), tskIDLE_PRIORITY + 2, &initParam->test_thread_handle);
            TEST_ASSERT_MESSAGE(pdPASS == result, "Could not create worker!");
#else
            initParam->task_stack_base = malloc(TEST_THREAD_SAFETY_TASK_STACK_SIZE * sizeof(StackType_t));
            TEST_ASSERT_MESSAGE(NULL != initParam->task_stack_base, "Out of memory");
            initParam->test_thread_handle = xTaskCreateStatic(sendRecvTestTask, "THREAD_SAFETY_TASK", TEST_THREAD_SAFETY_TASK_STACK_SIZE, (void *)(initParam), tskIDLE_PRIORITY + 2, initParam->task_stack_base, &initParam->xTaskBuffer);
#endif
#elif defined(FSL_RTOS_XOS)
            initParam->task_stack_base = malloc(XOS_STACK_MIN_SIZE + 4*TEST_THREAD_SAFETY_TASK_STACK_SIZE);
            TEST_ASSERT_MESSAGE(NULL != initParam->task_stack_base, "Out of memory");
            result = xos_thread_create(&initParam->test_thread_handle, NULL, (XosThreadFunc*)sendRecvTestTask, (void *)(initParam), "THREAD_SAFETY_TASK", initParam->task_stack_base, (uint32_t)(XOS_STACK_MIN_SIZE + 4*TEST_THREAD_SAFETY_TASK_STACK_SIZE), 6, 0, 0);
            TEST_ASSERT_MESSAGE(XOS_OK == result, "Could not create worker!");
#elif defined(FSL_RTOS_THREADX)
            initParam->task_stack_base = env_allocate_memory(TEST_THREAD_SAFETY_TASK_STACK_SIZE * sizeof(ULONG));
            TEST_ASSERT_MESSAGE(NULL != initParam->task_stack_base, "Out of memory");
            result = tx_thread_create(&initParam->test_thread_handle, "THREAD_SAFETY_TASK", sendRecvTestTask, (ULONG)(initParam), (VOID *)initParam->task_stack_base, (TEST_THREAD_SAFETY_TASK_STACK_SIZE * sizeof(ULONG)), (TX_MAX_PRIORITIES - 2), (TX_MAX_PRIORITIES - 2), 1, TX_AUTO_START);
            TEST_ASSERT_MESSAGE(TX_SUCCESS == result, "Could not create worker!");
#endif
        }

        globCntr = 0;
        while (globCntr < WORKER_NUMBER)
        {
#if defined(SDK_OS_FREE_RTOS)
            vTaskDelay(100);
#elif defined(FSL_RTOS_XOS)
            xos_thread_sleep_msec(1000);
#elif defined(FSL_RTOS_THREADX)
            tx_thread_sleep(100);
#endif
        }

        /* Send command to end to the other core to finish testing */
        msg.CMD = CTR_CMD_FINISH;
        msg.ACK_REQUIRED = ACK_REQUIRED_NO;
        rpmsg_lite_send(my_rpmsg, ctrl_ept, TC_REMOTE_EPT_ADDR, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    }
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
}

#if defined(FSL_RTOS_THREADX)
VOID sendRecvTestTask(ULONG initParam)
#else
void sendRecvTestTask(void *initParam)
#endif
{
    initParamTypedef *init = (initParamTypedef *)initParam;
    TEST_ASSERT_MESSAGE(NULL != init, "Out of memory");
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    rpmsg_queue_handle sender_q = NULL;
    rpmsg_static_queue_ctxt sender_queue_ctxt = {0};
    uint8_t sender_queue_storage[RL_ENV_QUEUE_STATIC_STORAGE_SIZE] = {0};
    struct rpmsg_lite_endpoint *sender_ept = NULL;
    struct rpmsg_lite_ept_static_context sender_ept_ctxt = {0};
    uint32_t responder_ept_addr = -1;
    uint32_t ept_address = TEST_EPT_NUM_BASE + init->worker_id;
#ifndef UNITY_NOT_PRINT_LOG
    char worker_id_str[2] = {init->worker_id + 'a', '\0'};
#endif
    int32_t ret_value, i = 0, testing_count = 0;
    uint32_t num_of_received_bytes = 0;
    uint32_t src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    char recv_buffer[SENDER_APP_BUF_SIZE];

    data_create_ept_param.ept_to_ack_addr = init->ack_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    env_memcpy((void *)data_send_param.msg, "abc", (uint32_t)4);
    data_send_param.ept_to_ack_addr = init->ack_ept->addr;
    data_send_param.msg_size = CMD_SEND_MSG_SIZE;
    data_send_param.repeat_count = 1;
    data_send_param.mode = CMD_SEND_MODE_COPY;

    data_recv_param.ept_to_ack_addr = init->ack_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = RL_BLOCK;
    data_recv_param.mode = CMD_RECV_MODE_COPY;

    /* Testing with blocking call and non-blocking call (timeout = 0) */
    for (testing_count = 0; testing_count < 16; testing_count++)
    {
        UnityPrint(worker_id_str); /* Print a dot, showing progress */

        for (i = 0; i < TEST_CNT; i++)
        {
            /*
             * Test receive function
             * Sender sends a request to create endpoint on the other side
             * Responder will receive the data from sender through this endpoint
             */
            msg.CMD = CTR_CMD_CREATE_EP;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param),
                       sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
            /* Send command to create endpoint to the other core */
            ret_value = rpmsg_lite_send(my_rpmsg, init->ack_ept, TC_REMOTE_EPT_ADDR, (char *)&msg,
                                        sizeof(CONTROL_MESSAGE), RL_BLOCK);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to send CTR_CMD_CREATE_EP command to other side");
            /* Get respond from other side */
            ret_value = rpmsg_queue_recv(my_rpmsg, init->ack_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                         &num_of_received_bytes, RL_BLOCK);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : 0),
                                "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : 0), "error! failed to create endpoint on other side");
            env_memcpy((void *)&responder_ept_addr, (void *)ack_msg.RESP_DATA, (uint32_t)(sizeof(uint32_t)));
            data_recv_param.responder_ept_addr = responder_ept_addr;

            /* send CTR_CMD_RECV command to the other side */
            msg.CMD = CTR_CMD_RECV;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)&data_recv_param,
                       (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
            ret_value = rpmsg_lite_send(my_rpmsg, init->ack_ept, TC_REMOTE_EPT_ADDR, (char *)&msg,
                                        sizeof(CONTROL_MESSAGE), RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0), "error! failed to send CTR_CMD_RECV command to other side");

            /* Send "aaa" string to other side */
            ret_value = rpmsg_lite_send(my_rpmsg, init->ack_ept, data_recv_param.responder_ept_addr, (char *)"aaa", 3,
                                        RL_BLOCK);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0), "error! failed to send 'aaa' string to other side");

            /* Get respond from other core */
            ret_value = rpmsg_queue_recv(my_rpmsg, init->ack_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                         &num_of_received_bytes, RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : 0),
                                "error! expecting acknowledge of CTR_CMD_RECV copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : 0),
                                "error! failed when call rpmsg_rtos_recv function on the other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(ack_msg.RESP_DATA, "aaa", 3) ? 1 : 0), "error! incorrect data received");

            /*
             * Test send function
             * Create a new endpoint on the sender side and sender will receive data through this endpoint
             */
            sender_q = rpmsg_queue_create(my_rpmsg, &sender_queue_storage[0], &sender_queue_ctxt);
            TEST_ASSERT_MESSAGE(NULL != sender_q, "error! failed to create queue");
            sender_ept = rpmsg_lite_create_ept(my_rpmsg, ept_address, rpmsg_queue_rx_cb, sender_q, &sender_ept_ctxt);
            TEST_ASSERT_MESSAGE(NULL != sender_ept, "error! failed to create endpoint");

            data_send_param.dest_addr = sender_ept->addr;

            msg.CMD = CTR_CMD_SEND;
            msg.ACK_REQUIRED = ACK_REQUIRED_NO;
            env_memcpy((void *)msg.DATA, (void *)&data_send_param,
                       (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
            ret_value = rpmsg_lite_send(my_rpmsg, init->ack_ept, TC_REMOTE_EPT_ADDR, (char *)&msg,
                                        sizeof(CONTROL_MESSAGE), RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0), "error! failed to send CTR_CMD_SEND command to other side");

            ret_value = rpmsg_queue_recv(my_rpmsg, sender_q, &src, (char *)recv_buffer, SENDER_APP_BUF_SIZE,
                                         &num_of_received_bytes, RL_BLOCK);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0), "error! failed to receive data from other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(recv_buffer, "abc", 3) ? 1 : 0), "error! incorrect data received");

            /*
             * Destroy created endpoint on the sender side
             */
            rpmsg_lite_destroy_ept(my_rpmsg, sender_ept);
            rpmsg_queue_destroy(my_rpmsg, sender_q);
            sender_q = NULL;
            sender_ept = NULL;

            ts_destroy_ept(responder_ept_addr, init->ack_ept, init->ack_q);
        }

        /*
         * Attempt to call receive function on the other side with the invalid EP pointer (not yet created EP)
         */
        data_recv_param.responder_ept_addr = -1;
        msg.CMD = CTR_CMD_RECV;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
        ret_value = rpmsg_lite_send(my_rpmsg, init->ack_ept, TC_REMOTE_EPT_ADDR, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                    RL_BLOCK);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0), "error! failed to send CTR_CMD_RECV command to other side");
        /* Get respond from other side */
        ret_value = rpmsg_queue_recv(my_rpmsg, init->ack_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                     &num_of_received_bytes, RL_BLOCK);

        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0), "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : 0),
                            "error! expecting acknowledge of CTR_CMD_RECV copmmand");
        TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE < 0 ? 1 : 0),
                            "error! failed when call rpmsg_rtos_recv function on the other side");
    }
    globCntr++;

    rpmsg_lite_destroy_ept(my_rpmsg, init->ack_ept);
    rpmsg_queue_destroy(my_rpmsg, init->ack_q);
#if defined(SDK_OS_FREE_RTOS)
    free(init);
    vTaskDelete(NULL);
#elif defined(FSL_RTOS_XOS)
    free(init->task_stack_base);
    free(init);
    xos_thread_exit(0);
#elif defined(FSL_RTOS_THREADX)
    env_free_memory(init->task_stack_base);
    env_free_memory(init);
    tx_thread_delete(&(init->test_thread_handle));
#endif
}

void run_tests(void *unused)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("05_thread_safety_rtos_sec_core");
    __coveragescanner_install("05_thread_safety_rtos_sec_core.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(tc_1_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
