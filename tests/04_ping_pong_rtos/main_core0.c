/*
 * Copyright 2016-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rpmsg_lite.h"
#include <stdlib.h>
#include <string.h>
#include "pingpong_common.h"
#include "unity.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#include "assert.h"
#include "app.h"
#if defined(SDK_OS_FREE_RTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TC_TRANSFER_COUNT 10
#define DATA_LEN 45
#define TC_LOCAL_EPT_ADDR (40)
//#define TC_REMOTE_EPT_ADDR (30)

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
rpmsg_queue_handle ctrl_q = NULL;
struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
rpmsg_ns_handle ns_handle = NULL;
volatile uint32_t remote_addr = 0U;

static void app_nameservice_isr_cb(uint32_t new_ept, const char *new_ept_name, uint32_t flags, void *user_data)
{
    uint32_t *data = (uint32_t *)user_data;

    *data = new_ept;
}

/*
 * utility: initialize rpmsg and environment
 * and wait for default channel
 */
int32_t ts_init_rpmsg(void)
{
#if defined(SDK_OS_FREE_RTOS)
    HeapStats_t xHeapStats;
    void *t;
    /* static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );*/
    size_t xHeapStructSizeTemp = ( sizeof( uint32_t* ) + sizeof( size_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );
#endif /* SDK_OS_FREE_RTOS */

    env_init();
    //env_sleep_msec(200);
#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
#if defined(SDK_OS_FREE_RTOS)
    // Simulate out of heap memory cases
    // Force rpmsg_lite_instance allocation fail
    vPortGetHeapStats(&xHeapStats);
    t = env_allocate_memory(xHeapStats.xAvailableHeapSpaceInBytes - xHeapStructSizeTemp - 1U);
    my_rpmsg = rpmsg_lite_master_init(rpmsg_lite_base, SH_MEM_TOTAL_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS);
    env_free_memory(t);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "'rpmsg_lite_master_init' with no free heap failed");

    // Force allocation fail in first virtqueue_create call
    vPortGetHeapStats(&xHeapStats);
    t = env_allocate_memory(xHeapStats.xAvailableHeapSpaceInBytes - (2*xHeapStructSizeTemp) - sizeof(struct rpmsg_lite_instance) - 8U - 1U);
    my_rpmsg = rpmsg_lite_master_init(rpmsg_lite_base, SH_MEM_TOTAL_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS);
    env_free_memory(t);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "'rpmsg_lite_master_init' with no free heap failed");

    // Force allocation fail in second virtqueue_create call
    vPortGetHeapStats(&xHeapStats);
    t = env_allocate_memory(xHeapStats.xAvailableHeapSpaceInBytes - (2*xHeapStructSizeTemp) - sizeof(struct rpmsg_lite_instance) - 8U - sizeof(struct virtqueue) - 8U - 1U);
    my_rpmsg = rpmsg_lite_master_init(rpmsg_lite_base, SH_MEM_TOTAL_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS);
    env_free_memory(t);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "'rpmsg_lite_master_init' with no free heap failed");

    // Force allocation fail in env_create_mutex((LOCK *)&rpmsg_lite_dev->lock, 1)
    vPortGetHeapStats(&xHeapStats);
    t = env_allocate_memory(xHeapStats.xAvailableHeapSpaceInBytes - (2*xHeapStructSizeTemp) - sizeof(struct rpmsg_lite_instance) - 8U - (2*sizeof(struct virtqueue)) - 16U -  1U);
    my_rpmsg = rpmsg_lite_master_init(rpmsg_lite_base, SH_MEM_TOTAL_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS);
    env_free_memory(t);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "'rpmsg_lite_master_init' with no free heap failed");
#endif /* SDK_OS_FREE_RTOS */

    // Correct call
    my_rpmsg = rpmsg_lite_master_init(rpmsg_lite_base, SH_MEM_TOTAL_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS);
#else
    my_rpmsg = rpmsg_lite_master_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_SHMEM_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS);
#endif /* SH_MEM_NOT_TAKEN_FROM_LINKER */
    TEST_ASSERT_MESSAGE(NULL != my_rpmsg, "init function failed");

    //Register name service callback as soon as the rpmsg_lite_master_init() finishes
    //(virtqueue_kick unblocks the rpmsg_lite_wait_for_link_up() on the remote side and rpmsg_ns_announce is sent consequently)
#if defined(SDK_OS_FREE_RTOS)
    // Simulate out of heap memory cases
    // Force rpmsg_ns_callback_data allocation fail
    vPortGetHeapStats(&xHeapStats);
    t = env_allocate_memory(xHeapStats.xAvailableHeapSpaceInBytes - xHeapStructSizeTemp - 1U);
    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, (void *)&remote_addr);
    env_free_memory(t);
    TEST_ASSERT_MESSAGE(RL_NULL == ns_handle, "'rpmsg_ns_bind' with no free heap failed");
    // Force rpmsg_ns_context allocation fail
    vPortGetHeapStats(&xHeapStats);
    t = env_allocate_memory(xHeapStats.xAvailableHeapSpaceInBytes - (2*xHeapStructSizeTemp) - sizeof(struct rpmsg_ns_callback_data) - 8U - 1U);
    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, (void *)&remote_addr);
    env_free_memory(t);
    TEST_ASSERT_MESSAGE(RL_NULL == ns_handle, "'rpmsg_ns_bind' with no free heap failed");
#endif /* SDK_OS_FREE_RTOS */

    // Correct call
    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, (void *)&remote_addr);
    TEST_ASSERT_MESSAGE(NULL != ns_handle, "'rpmsg_ns_bind' failed");

    /* Wait until the secondary core application issues the nameservice isr and the remote endpoint address is known. */
    while (0U == remote_addr)
    {
    };

    ctrl_q = rpmsg_queue_create(my_rpmsg);
    TEST_ASSERT_MESSAGE(NULL != ctrl_q, "'rpmsg_queue_create' failed");

#if defined(SDK_OS_FREE_RTOS)
    // Simulate out of heap memory cases
    // Force rpmsg_lite_endpoint allocation fail
    vPortGetHeapStats(&xHeapStats);
    t = env_allocate_memory(xHeapStats.xAvailableHeapSpaceInBytes - xHeapStructSizeTemp - 1U);
    ctrl_ept = rpmsg_lite_create_ept(my_rpmsg, TC_LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, ctrl_q);
    env_free_memory(t);
    TEST_ASSERT_MESSAGE(RL_NULL == ctrl_ept, "'rpmsg_lite_create_ept' with no free heap failed");

    // Force llist allocation fail
    vPortGetHeapStats(&xHeapStats);
    t = env_allocate_memory(xHeapStats.xAvailableHeapSpaceInBytes - (2*xHeapStructSizeTemp) - sizeof(struct rpmsg_lite_endpoint) - 8U - 1U);
    ctrl_ept = rpmsg_lite_create_ept(my_rpmsg, TC_LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, ctrl_q);
    env_free_memory(t);
    TEST_ASSERT_MESSAGE(RL_NULL == ctrl_ept, "'rpmsg_lite_create_ept' with no free heap failed");
#endif /* SDK_OS_FREE_RTOS */

    // Correct call
    ctrl_ept = rpmsg_lite_create_ept(my_rpmsg, TC_LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, ctrl_q);
    TEST_ASSERT_MESSAGE(NULL != ctrl_ept, "'rpmsg_lite_create_ept' failed");

    // Invalid params for rpmsg_ns_bind
    ns_handle = rpmsg_ns_bind(my_rpmsg, RL_NULL, (void *)&remote_addr);
    TEST_ASSERT_MESSAGE(RL_NULL == ns_handle, "'rpmsg_ns_bind' with bad app_cb param failed");

    return 0;
}

/*
 * utility: deinitialize rpmsg and enviroment
 */
int32_t ts_deinit_rpmsg(void)
{
    rpmsg_lite_destroy_ept(my_rpmsg, ctrl_ept);
    rpmsg_queue_destroy(my_rpmsg, ctrl_q);
    rpmsg_ns_unbind(my_rpmsg, ns_handle);
    rpmsg_lite_deinit(my_rpmsg);
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
 * Destroy all created endpoints on the other side
 */
int32_t ts_destroy_epts()
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    int32_t ret_value;
    uint32_t num_of_received_bytes = 0;
    uint32_t src;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM data_destroy_ept_param;

    data_destroy_ept_param.ept_to_ack_addr = ctrl_ept->addr;
    data_destroy_ept_param.ept_to_destroy_addr = DESTROY_ALL_EPT;

    msg.CMD = CTR_CMD_DESTROY_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_destroy_ept_param), sizeof(CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM));
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);

    TEST_ASSERT_MESSAGE(0 == ret_value, "error! failed to send CTR_CMD_DESTROY_EP command to other side");
    /* Receive response from other core */
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 == ret_value, "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE(CTR_CMD_DESTROY_EP == ack_msg.CMD_ACK,
                        "error! expecting acknowledge of CTR_CMD_DESTROY_EP command");
    TEST_ASSERT_MESSAGE(0 == ack_msg.RETURN_VALUE, "error! failed to destroy endpoints on other side");

    return 0;
}

/*
 * Endpoint creation testing
 */
void tc_1_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    uint32_t responder_ept_addr;
    int32_t ret_value, i = 0;
    uint32_t num_of_received_bytes = 0;
    uint32_t src;
    uint32_t ept_address;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;

    ret_value = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == ret_value, "error! init function failed");
    if (ret_value)
    {
        ret_value = ts_deinit_rpmsg();
        TEST_ASSERT_MESSAGE(0 == ret_value, "error! deinit function failed");
        return;
    }

    data_create_ept_param.ept_to_ack_addr = ctrl_ept->addr;

    /*
     * EP creation testing with desired address of 1
     */
    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    data_create_ept_param.ept_to_create_addr = 1; /* Address to create endpoint */
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");
    /* Receive respond from other core */
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on the other side");

    /*
     * Creation of the same EP should not be possible - ERROR should be returned
     */
    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");
    /* Receive respond from other core */
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
    TEST_ASSERT_MESSAGE((0 != ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on other side");

    /*
     * Create endpoint with address = RL_ADDR_ANY
     */
    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    data_create_ept_param.ept_to_create_addr = RL_ADDR_ANY; /* Address to create endpoint */
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");

    /* Receive response from other core */
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on the other side");
    /* Get address of endpoint that was created with RL_ADDR_ANY parameter*/
    env_memcpy((void *)&responder_ept_addr, (void *)ack_msg.RESP_DATA, sizeof(uint32_t));
    ept_address = responder_ept_addr;

    /*
     * Create endpoint with address of endpoint that was created with RL_ADDR_ANY parameter
     * This should not be possible. FALSE will returned.
     */
    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    /* Address to create endpoint */
    data_create_ept_param.ept_to_create_addr = ept_address;
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");

    /* Receive respond from other core */
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
    TEST_ASSERT_MESSAGE((0 != ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on the other side");

    /*
     * Create another endpoints (TC_EPT_COUNT - 3)
     */
    for (i = 0; i < TC_EPT_COUNT - 3; i++)
    {
        msg.CMD = CTR_CMD_CREATE_EP;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        data_create_ept_param.ept_to_create_addr++;
        env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
        /* Send command to create endpoint to the other core */
        ret_value =
            rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to send CTR_CMD_CREATE_EP command to other side");
        /* Receive respond from other core */
        ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                     &num_of_received_bytes, RL_BLOCK);

        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                            "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
        TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to create endpoint on other side");
    }

    /*
     * Testing destroy endpoints
     */
    ts_destroy_epts();
}

/*
 * TEST #2: Testing RPMSG pingpong1
 * rpmsg_rtos_recv() and rpmsg_rtos_send() functions are called on
 * the other side. Both blocking and non-blocking modes of the rpmsg_rtos_recv() function are tested.
 */
void tc_2_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    uint32_t responder_ept_addr;
    struct rpmsg_lite_endpoint *sender_ept;
    rpmsg_queue_handle sender_q;
    uint32_t ept_address = INIT_EPT_ADDR;
    int32_t ret_value, i = 0, testing_count = 0;
    uint32_t num_of_received_bytes = 0;
    uint32_t src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    char recv_buffer[SENDER_APP_BUF_SIZE];

    data_create_ept_param.ept_to_ack_addr = ctrl_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    env_memcpy((void *)data_send_param.msg, "abc", (uint32_t)4);
    data_send_param.ept_to_ack_addr = ctrl_ept->addr;
    data_send_param.msg_size = CMD_SEND_MSG_SIZE;
    data_send_param.repeat_count = 1;
    data_send_param.mode = CMD_SEND_MODE_COPY;

    data_recv_param.ept_to_ack_addr = ctrl_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = RL_BLOCK;
    data_recv_param.mode = CMD_RECV_MODE_COPY;

    /* Testing with blocking call and non-blocking call (timeout = 0) */
    for (testing_count = 0; testing_count < 2; testing_count++)
    {
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
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                        RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_CREATE_EP command to other side");
            /* Get respond from other side */
            ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                         &num_of_received_bytes, RL_BLOCK);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint on other side");
            env_memcpy((void *)&responder_ept_addr, (void *)ack_msg.RESP_DATA, (uint32_t)(sizeof(uint32_t)));
            data_recv_param.responder_ept_addr = responder_ept_addr;

            /* send CTR_CMD_RECV command to the other side */
            msg.CMD = CTR_CMD_RECV;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)&data_recv_param,
                       (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                        RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_RECV command to other side");

            /* Send "aaa" string to other side */
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, responder_ept_addr, (char *)"aaa", 3, RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send 'aaa' string to other side");

            /* Get respond from other core */
            ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                         &num_of_received_bytes, RL_BLOCK);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_RECV command");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed when call rpmsg_rtos_recv function on the other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(ack_msg.RESP_DATA, "aaa", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Test send function
             * Create a new endpoint on the sender side and sender will receive data through this endpoint
             */
            sender_q = rpmsg_queue_create(my_rpmsg);
            TEST_ASSERT_MESSAGE((NULL != sender_q ? 1 : (0 != ts_destroy_epts())), "error! failed to create queue");
            sender_ept = rpmsg_lite_create_ept(my_rpmsg, ept_address, rpmsg_queue_rx_cb, sender_q);
            TEST_ASSERT_MESSAGE((NULL != sender_ept ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint");

            data_send_param.dest_addr = sender_ept->addr;

            msg.CMD = CTR_CMD_SEND;
            msg.ACK_REQUIRED = ACK_REQUIRED_NO;
            env_memcpy((void *)msg.DATA, (void *)&data_send_param,
                       (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                        RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_SEND command to other side");

            /* Wait untill the response msg is received, use rpmsg_queue_recv() with RL_DONT_BLOCK parameter then. */
            while(1 > rpmsg_queue_get_current_size(sender_q)) {};
            ret_value = rpmsg_queue_recv(my_rpmsg, sender_q, &src, (char *)&recv_buffer, SENDER_APP_BUF_SIZE,
                                         &num_of_received_bytes, RL_DONT_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive data from other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(recv_buffer, "abc", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            ret_value = rpmsg_queue_get_current_size(sender_q);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! expected number of messages in the sender queue is not equal to 0");

            /*
             * Destroy created endpoint on the sender side
             */
            rpmsg_lite_destroy_ept(my_rpmsg, sender_ept);
            rpmsg_queue_destroy(my_rpmsg, sender_q);

            /*
             * Destroy created endpoint on the other side
             */
            ts_destroy_epts();
        }

        /*
         * Attempt to call receive function on the other side with the invalid EP pointer (not yet created EP)
         */
        data_recv_param.responder_ept_addr = 123456; /* invalid non-existing addr */
        msg.CMD = CTR_CMD_RECV;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
        ret_value =
            rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to send CTR_CMD_RECV command to other side");
        /* Get respond from other side */
        ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                     &num_of_received_bytes, RL_BLOCK);

        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                            "error! expecting acknowledge of CTR_CMD_RECV command");
        TEST_ASSERT_MESSAGE((RL_ERR_PARAM == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                            "error! failed when call rpmsg_rtos_recv function on the other side");

        /*
         * Test receive function on the other side with non-blocking call (timeout = 0)
         */
        data_recv_param.timeout_ms = RL_DONT_BLOCK;
    }
}

/*
 * TEST #3: Testing RPMSG pingpong2
 * rpmsg_rtos_recv_nocopy() and rpmsg_rtos_send() functions are called on
 * the other side. Both blocking and non-blocking modes of the rpmsg_rtos_recv_nocopy() function are tested.
 */
void tc_3_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    uint32_t responder_ept_addr;
    struct rpmsg_lite_endpoint *sender_ept;
    rpmsg_queue_handle sender_q;
    uint32_t ept_address = INIT_EPT_ADDR;
    int32_t ret_value, i = 0, testing_count = 0;
    uint32_t num_of_received_bytes = 0;
    uint32_t src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    char recv_buffer[SENDER_APP_BUF_SIZE];

    data_create_ept_param.ept_to_ack_addr = ctrl_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    env_memcpy((void *)data_send_param.msg, "abc", (uint32_t)4);
    data_send_param.ept_to_ack_addr = ctrl_ept->addr;
    data_send_param.msg_size = CMD_SEND_MSG_SIZE;
    data_send_param.repeat_count = 1;
    data_send_param.mode = CMD_SEND_MODE_COPY;

    data_recv_param.ept_to_ack_addr = ctrl_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = RL_BLOCK;
    data_recv_param.mode = CMD_RECV_MODE_NOCOPY;

    /* Testing with blocking call and non-blocking call (timeout = 0) */
    for (testing_count = 0; testing_count < 2; testing_count++)
    {
        for (i = 0; i < TEST_CNT; i++)
        {
            /*
             * Test receive function
             * Sender sends a request to the responder to create endpoint on other side
             * Responder will receive the data from sender through this endpoint
             */
            msg.CMD = CTR_CMD_CREATE_EP;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param),
                       sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
            /* Send command to create endpoint to the other core */
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                        RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_CREATE_EP command to responder");
            ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                         &num_of_received_bytes, RL_BLOCK);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from responder");
            TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint on other side");
            env_memcpy((void *)&responder_ept_addr, (void *)ack_msg.RESP_DATA, (uint32_t)(sizeof(uint32_t)));
            data_recv_param.responder_ept_addr = responder_ept_addr;

            msg.CMD = CTR_CMD_RECV;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)&data_recv_param,
                       (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                        RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_RECV command to other side");

            /* Send "aaa" string to other side */
            ret_value =
                rpmsg_lite_send(my_rpmsg, ctrl_ept, data_recv_param.responder_ept_addr, (char *)"aaa", 3, RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send 'aaa' string to other side");

            /* Get respond from other side */
            ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                         &num_of_received_bytes, RL_BLOCK);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_RECV command");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed when call rpmsg_rtos_recv function on the other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(ack_msg.RESP_DATA, "aaa", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Test send function
             * Create a new endpoint on the sender side and sender will receive data through this endpoint
             */
            sender_q = rpmsg_queue_create(my_rpmsg);
            TEST_ASSERT_MESSAGE((NULL != sender_q ? 1 : (0 != ts_destroy_epts())), "error! failed to create queue");
            sender_ept = rpmsg_lite_create_ept(my_rpmsg, ept_address, rpmsg_queue_rx_cb, sender_q);
            TEST_ASSERT_MESSAGE((NULL != sender_ept ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint");

            data_send_param.dest_addr = sender_ept->addr;

            msg.CMD = CTR_CMD_SEND;
            msg.ACK_REQUIRED = ACK_REQUIRED_NO;
            env_memcpy((void *)msg.DATA, (void *)&data_send_param,
                       (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                        RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_SEND command to other side");

            ret_value = rpmsg_queue_recv(my_rpmsg, sender_q, &src, (char *)recv_buffer, SENDER_APP_BUF_SIZE,
                                         &num_of_received_bytes, RL_BLOCK);

            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive data from other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(recv_buffer, "abc", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Destroy created endpoint on the sender side
             */
            rpmsg_lite_destroy_ept(my_rpmsg, sender_ept);
            rpmsg_queue_destroy(my_rpmsg, sender_q);

            /*
             * Destroy created endpoint on the other side
             */
            ts_destroy_epts();
        }

        /*
         * Attempt to call receive function on the other side with the invalid EP pointer (not yet created EP)
         */
        data_recv_param.responder_ept_addr = 123456; /* non-existing ept addr */
        msg.CMD = CTR_CMD_RECV;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
        ret_value =
            rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to send CTR_CMD_RECV command to other side");
        /* Get respond from other side */
        ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                     &num_of_received_bytes, RL_BLOCK);

        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                            "error! expecting acknowledge of CTR_CMD_RECV command");
        TEST_ASSERT_MESSAGE((RL_ERR_PARAM == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                            "error! failed when call rpmsg_rtos_recv function on the other side");

        /*
         * Test receive function on the other side with non-blocking call (timeout = 0)
         */
        data_recv_param.timeout_ms = RL_DONT_BLOCK;
    }
}

/*
 * TEST #4: Testing timeout for receive function with copy mode
 */
void tc_4_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    uint32_t responder_ept_addr;
    uint32_t ept_address = INIT_EPT_ADDR;
    int32_t ret_value;
    uint32_t num_of_received_bytes = 0;
    uint32_t src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;

    data_create_ept_param.ept_to_ack_addr = ctrl_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    data_recv_param.ept_to_ack_addr = ctrl_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = CMD_RECV_TIMEOUT_MS;
    data_recv_param.mode = CMD_RECV_MODE_COPY;

    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on other side");
    env_memcpy((void *)&responder_ept_addr, (void *)ack_msg.RESP_DATA,
               (uint32_t)(sizeof(struct rpmsg_lite_endpoint *)));
    data_recv_param.responder_ept_addr = responder_ept_addr;

    /* Wait for a new message until the timeout expires, no message is sent. */
    msg.CMD = CTR_CMD_RECV;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_RECV command to other side");
    /* Get respond from other side */
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_RECV command");
    TEST_ASSERT_MESSAGE((0 != ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed when call rpmsg_rtos_recv function on the other side");
    TEST_ASSERT_INT_WITHIN_MESSAGE(CMD_RECV_TIMEOUT_MS * 0.2, CMD_RECV_TIMEOUT_MS, ack_msg.TIMEOUT_MSEC,
                                   "error! incorrect timeout received");

    /* Wait for a new message until the timeout expires, a message is sent at half of the timeout. */
    msg.CMD = CTR_CMD_RECV;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_RECV command to other side");
    env_sleep_msec(CMD_RECV_TIMEOUT_MS / 2);
    /* Send "aaa" string to other side */
    ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_recv_param.responder_ept_addr, (char *)"aaa", 3, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send 'aaa' string to other side");
    /* Get respond from other side */
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_RECV command");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed when call rpmsg_rtos_recv function on the other side");
    TEST_ASSERT_INT_WITHIN_MESSAGE((CMD_RECV_TIMEOUT_MS / 2) * 0.2, CMD_RECV_TIMEOUT_MS / 2, ack_msg.TIMEOUT_MSEC,
                                   "error! incorrect timeout received");

    /*
     * Destroy created endpoint on the other side
     */
    ts_destroy_epts();
}

/*
 * Testing timeout for receive function with no-copy mode
 */
void tc_5_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    uint32_t responder_ept_addr;
    uint32_t ept_address = INIT_EPT_ADDR;
    int32_t ret_value;
    uint32_t num_of_received_bytes = 0;
    uint32_t src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;

    data_create_ept_param.ept_to_ack_addr = ctrl_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    data_recv_param.ept_to_ack_addr = ctrl_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = CMD_RECV_TIMEOUT_MS;
    data_recv_param.mode = CMD_RECV_MODE_NOCOPY;

    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on other side");
    env_memcpy((void *)&responder_ept_addr, (void *)ack_msg.RESP_DATA,
               (uint32_t)(sizeof(struct rpmsg_lite_endpoint *)));
    data_recv_param.responder_ept_addr = responder_ept_addr;

    /* Wait for a new message until the timeout expires, no message is sent. */
    msg.CMD = CTR_CMD_RECV;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_RECV command to other side");
    /* Get respond from other side */
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_RECV command");
    TEST_ASSERT_MESSAGE((0 != ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed when call rpmsg_rtos_recv function on the other side");
    TEST_ASSERT_INT_WITHIN_MESSAGE(CMD_RECV_TIMEOUT_MS * 0.2, CMD_RECV_TIMEOUT_MS, ack_msg.TIMEOUT_MSEC,
                                   "error! incorrect timeout received");

    /* Wait for a new message until the timeout expires, a message is sent at half of the timeout. */
    msg.CMD = CTR_CMD_RECV;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
    ret_value =
        rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_RECV command to other side");
    env_sleep_msec(CMD_RECV_TIMEOUT_MS / 2);
    /* Send "aaa" string to other side */
    ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_recv_param.responder_ept_addr, (char *)"aaa", 3, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send 'aaa' string to other side");
    /* Get respond from other side */
    ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                 &num_of_received_bytes, RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_RECV command");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed when call rpmsg_rtos_recv on the other side");
    TEST_ASSERT_INT_WITHIN_MESSAGE((CMD_RECV_TIMEOUT_MS / 2) * 0.2, CMD_RECV_TIMEOUT_MS / 2, ack_msg.TIMEOUT_MSEC,
                                   "error! incorrect timeout received");

    /*
     * Destroy created endpoint on the other side
     */
    ts_destroy_epts();
}

/*
 * TEST #6: Testing RPMSG pingpong3
 * rpmsg_rtos_recv_nocopy() and rpmsg_rtos_send_nocopy() functions are called on
 * the other side. Both blocking and non-blocking modes of the rpmsg_rtos_recv_nocopy() function are tested.
 */
void tc_6_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    uint32_t responder_ept_addr;
    struct rpmsg_lite_endpoint *sender_ept;
    rpmsg_queue_handle sender_q;
    uint32_t ept_address = INIT_EPT_ADDR;
    int32_t ret_value, i = 0, testing_count = 0;
    uint32_t num_of_received_bytes = 0;
    uint32_t src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    char recv_buffer[SENDER_APP_BUF_SIZE];

    data_create_ept_param.ept_to_ack_addr = ctrl_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    env_memcpy((void *)data_send_param.msg, "abc", (uint32_t)4);
    data_send_param.ept_to_ack_addr = ctrl_ept->addr;
    data_send_param.msg_size = CMD_SEND_MSG_SIZE;
    data_send_param.repeat_count = 1;
    data_send_param.mode = CMD_SEND_MODE_NOCOPY;

    data_recv_param.ept_to_ack_addr = ctrl_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = RL_BLOCK;
    data_recv_param.mode = CMD_RECV_MODE_NOCOPY;

    /* Testing with blocking call and non-blocking call (timeout = 0) */
    for (testing_count = 0; testing_count < 2; testing_count++)
    {
        for (i = 0; i < TEST_CNT; i++)
        {
            /*
             * Test receive function
             * Sender sends a request to the responder to create endpoint on other side
             * Responder will receive the data from sender through this endpoint
             */
            msg.CMD = CTR_CMD_CREATE_EP;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param),
                       sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
            /* Send command to create endpoint to the other core */
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                        RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_CREATE_EP command to responder");
            ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                         &num_of_received_bytes, RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from responder");
            TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_CREATE_EP command");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint on other side");
            env_memcpy((void *)&responder_ept_addr, (void *)ack_msg.RESP_DATA,
                       (uint32_t)(sizeof(struct rpmsg_lite_endpoint *)));
            data_recv_param.responder_ept_addr = responder_ept_addr;

            msg.CMD = CTR_CMD_RECV;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)&data_recv_param,
                       (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                        RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_RECV command to other side");

            /* Send "aaa" string to other side */
            ret_value =
                rpmsg_lite_send(my_rpmsg, ctrl_ept, data_recv_param.responder_ept_addr, (char *)"aaa", 3, RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send 'aaa' string to other side");

            /* Get respond from other side */
            ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                         &num_of_received_bytes, RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_RECV command");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed when call rpmsg_rtos_recv function on the other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(ack_msg.RESP_DATA, "aaa", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Test send function
             * Create a new endpoint on the sender side and sender will receive data through this endpoint
             */
            ept_address = INIT_EPT_ADDR + 1;

            sender_q = rpmsg_queue_create(my_rpmsg);
            TEST_ASSERT_MESSAGE((NULL != sender_q ? 1 : (0 != ts_destroy_epts())), "error! failed to create queue");
            sender_ept = rpmsg_lite_create_ept(my_rpmsg, ept_address, rpmsg_queue_rx_cb, sender_q);
            TEST_ASSERT_MESSAGE((NULL != sender_ept ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint");

            data_send_param.dest_addr = sender_ept->addr;

            msg.CMD = CTR_CMD_SEND;
            msg.ACK_REQUIRED = ACK_REQUIRED_NO;
            env_memcpy((void *)msg.DATA, (void *)&data_send_param,
                       (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
            ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                        RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_SEND command to other side");

            ret_value = rpmsg_queue_recv(my_rpmsg, sender_q, &src, (char *)recv_buffer, SENDER_APP_BUF_SIZE,
                                         &num_of_received_bytes, RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive data from other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(recv_buffer, "abc", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Destroy created endpoint on the sender side
             */
            rpmsg_lite_destroy_ept(my_rpmsg, sender_ept);
            rpmsg_queue_destroy(my_rpmsg, sender_q);

            /*
             * Destroy created endpoint on the other side
             */
            ts_destroy_epts();
        }

        /*
         * Attempt to call receive function on the other side with the invalid EP pointer (not yet created EP)
         */
        data_recv_param.responder_ept_addr = 123456; /* non-existing ept addr */
        msg.CMD = CTR_CMD_RECV;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (uint32_t)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
        ret_value =
            rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE), RL_BLOCK);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to send CTR_CMD_RECV command to other side");
        /* Get respond from other side */
        ret_value = rpmsg_queue_recv(my_rpmsg, ctrl_q, &src, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                     &num_of_received_bytes, RL_BLOCK);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                            "error! expecting acknowledge of CTR_CMD_RECV command");
        TEST_ASSERT_MESSAGE((RL_ERR_PARAM == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                            "error! failed when call rpmsg_rtos_recv function on the other side");

        /*
         * Test receive function on the other side with non-blocking call (timeout = 0)
         */
        data_recv_param.timeout_ms = RL_DONT_BLOCK;
    }
}

void tc_7_finish_task(void)
{
    CONTROL_MESSAGE msg = {0};
    msg.CMD = CTR_CMD_FINISH;
    msg.ACK_REQUIRED = ACK_REQUIRED_NO;
    int32_t ret_value;
    ret_value = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&msg, sizeof(CONTROL_MESSAGE),
                                RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_FINISH command to other side");

}

void run_tests(void *unused)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("04_ping_pong_rtos");
    __coveragescanner_install("04_ping_pong_rtos.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(tc_1_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_EXAMPLE(tc_2_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    RUN_EXAMPLE(tc_3_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 2));
    RUN_EXAMPLE(tc_4_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 3));
    RUN_EXAMPLE(tc_5_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 4));
    RUN_EXAMPLE(tc_6_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 5));
    RUN_EXAMPLE(tc_7_finish_task, MAKE_UNITY_NUM(k_unity_rpmsg, 6));
}
