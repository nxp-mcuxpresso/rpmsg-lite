/*
 * Copyright 2016-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rpmsg_lite.h"
#include "rpmsg_ns.h"
#include <string.h>
#include "unity.h"
#include "assert.h"
#include "app.h"
#include "pingpong_common.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
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
/* This is for the responder side */
volatile int32_t message_received = 0;
volatile int32_t message_received_2 = 0;
ACKNOWLEDGE_MESSAGE ack_msg = {0};
char trans_data[100] = {0};
struct rpmsg_lite_endpoint *ctrl_ept = {0};
struct rpmsg_lite_endpoint *ept_with_addr_1 = {0};
struct rpmsg_lite_ept_static_context ctrl_ept_ctxt;
struct rpmsg_lite_ept_static_context ept_with_addr_1_ctxt;
struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
struct rpmsg_lite_instance rpmsg_ctxt;
rpmsg_ns_handle ns_handle = NULL;
rpmsg_ns_static_context nameservice_ept_ctxt;
volatile uint32_t remote_addr = 0U;

static void app_nameservice_isr_cb(uint32_t new_ept, const char *new_ept_name, uint32_t flags, void *user_data)
{
    uint32_t *data = (uint32_t *)user_data;

    *data = new_ept;
}


// control ept callback
int32_t test_read_cb(void *payload, uint32_t payload_len, uint32_t src, void *priv)
{
    TEST_ASSERT_MESSAGE(0 == message_received, "interrupt missed");
    ack_msg = *((ACKNOWLEDGE_MESSAGE_PTR)payload);
    message_received = 1;
    return RL_RELEASE;
}

// custom ept callback
int32_t ept_cb(void *payload, uint32_t payload_len, uint32_t src, void *priv)
{
    TEST_ASSERT_MESSAGE(0 == message_received_2, "interrupt missed");
    env_memcpy((void *)trans_data, payload, payload_len);
    message_received_2 = 1;
    return RL_RELEASE;
}

// utility: initialize rpmsg and environment
// and wait for default channel
int32_t ts_init_rpmsg(void)
{
    env_init();
#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
    my_rpmsg = rpmsg_lite_master_init(rpmsg_lite_base, SH_MEM_TOTAL_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS, &rpmsg_ctxt);
#else
    my_rpmsg = rpmsg_lite_master_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_SHMEM_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS, &rpmsg_ctxt);
#endif /* SH_MEM_NOT_TAKEN_FROM_LINKER */
    TEST_ASSERT_MESSAGE(NULL != my_rpmsg, "init function failed");

    //Register name service callback as soon as the rpmsg_lite_master_init() finishes
    //(virtqueue_kick unblocks the rpmsg_lite_wait_for_link_up() on the remote side and rpmsg_ns_announce is sent consequently)
    // invalid params for rpmsg_ns_bind
    ns_handle = rpmsg_ns_bind(my_rpmsg, RL_NULL, (void *)&remote_addr, &nameservice_ept_ctxt);
    TEST_ASSERT_MESSAGE(RL_NULL == ns_handle, "'rpmsg_ns_bind' with bad app_cb param failed");
    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, ((void *)0), RL_NULL);
    TEST_ASSERT_MESSAGE(RL_NULL == ns_handle, "'rpmsg_ns_bind' with bad ns_ept_ctxt param failed");
    
    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, (void *)&remote_addr, &nameservice_ept_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ns_handle, "'rpmsg_ns_bind' failed");

    /* Wait until the secondary core application issues the nameservice isr and the remote endpoint address is known. */
    while (0U == remote_addr)
    {
    };

    ctrl_ept = rpmsg_lite_create_ept(my_rpmsg, TC_LOCAL_EPT_ADDR, test_read_cb, NULL, &ctrl_ept_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ctrl_ept, "'rpmsg_lite_create_ept' failed");
    
    /* This endpoint is created just to force the for loop iteration in rpmsg_lite_create_ept when RL_ADDR_ANY address is passed as the parameter */
    ept_with_addr_1 = rpmsg_lite_create_ept(my_rpmsg, 1, test_read_cb, NULL, &ept_with_addr_1_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ept_with_addr_1, "'rpmsg_lite_create_ept' failed");
    
    return 0;
}

// utility: deinitialize rpmsg and environment
int32_t ts_deinit_rpmsg(void)
{
    rpmsg_lite_destroy_ept(my_rpmsg, ctrl_ept);
    rpmsg_lite_destroy_ept(my_rpmsg, ept_with_addr_1);
    rpmsg_ns_unbind(my_rpmsg, ns_handle);
    rpmsg_lite_deinit(my_rpmsg);
    ept_with_addr_1 = NULL;
    ctrl_ept = NULL;
    remote_addr = 0U;
    ns_handle = NULL;
    my_rpmsg = NULL;
    return 0;
}

// this test case is to test the endpoint functionality.
void tc_1_create_delete_ep_cmd_sender(void)
{
    void *mutex = NULL;
    int32_t result = 0;
    uint32_t data = EP_SIGNATURE;
    uint32_t dest = 0;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ep_param;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM data_destroy_ep_param;
    CONTROL_MESSAGE control_msg = {0};

    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to init");
    if (result)
        goto end;

    result = env_create_mutex(&mutex, 1, NULL);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to create mutex");
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);

    // create ep command
    data_create_ep_param.ept_to_ack_addr = ctrl_ept->addr;
    data_create_ep_param.ept_to_create_addr = RL_ADDR_ANY;
    control_msg.CMD = CTR_CMD_CREATE_EP;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_create_ep_param),
               sizeof(struct control_message_data_create_ept_param));
    result = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&control_msg,
                             sizeof(struct control_message), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);

    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of create endpoint command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! create endpoint command operation in other side failed");
    dest = ack_msg.RESP_DATA[0];
    /* send message to newly created endpoint */
    result = rpmsg_lite_send(my_rpmsg, ctrl_ept, dest, (char *)&data, sizeof(uint32_t), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");

    // create another endpoint with same address
    data_create_ep_param.ept_to_create_addr = dest;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_create_ep_param),
               sizeof(struct control_message_data_create_ept_param));
    result = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&control_msg,
                             sizeof(struct control_message), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of create endpoint command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE != 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! create new endpoint with same address as another endpoint in the other side failed");

    // delete created endpoint
    /* destroy endpoint not created yet */
    data_destroy_ep_param.ept_to_ack_addr = ctrl_ept->addr;
    data_destroy_ep_param.ept_to_destroy_addr = dest + 1;
    control_msg.CMD = CTR_CMD_DESTROY_EP;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_destroy_ep_param),
               sizeof(struct control_message_data_destroy_ept_param));
    result = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&control_msg,
                             sizeof(struct control_message), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_DESTROY_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of destroy endpoint command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE != 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! destroy an unexisted endpoint failed");
    /* destroy endpoint created before */
    data_destroy_ep_param.ept_to_destroy_addr = dest;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_destroy_ep_param),
               sizeof(struct control_message_data_destroy_ept_param));
    result = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&control_msg,
                             sizeof(struct control_message), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_DESTROY_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of destroy endpoint command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! destroy an endpoint failed");

    env_delete_mutex(mutex);
end:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "system clean up error");
}

// this test case is to test the send functionality.
void tc_2_send_cmd_sender(void)
{
    void *mutex = NULL;
    int32_t result = 0;
    char data[10] = "abc";
    char data_2[15] = "abc nocopy";
    CONTROL_MESSAGE control_msg = {0};
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param = {0};
    struct rpmsg_lite_endpoint *my_ept = {0};
    struct rpmsg_lite_ept_static_context my_ept_ctxt;

    result = env_create_mutex(&mutex, 1, NULL);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to create mutex");

    env_lock_mutex(mutex);
    message_received = 0;
    message_received_2 = 0;
    env_unlock_mutex(mutex);

    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to init");
    if (result)
        goto end;

    // this custom endpoint is to receive the data from responder.
    my_ept = rpmsg_lite_create_ept(my_rpmsg, RL_ADDR_ANY, ept_cb, NULL, &my_ept_ctxt);
    TEST_ASSERT_MESSAGE((NULL != my_ept ? 1 : (0 != ts_deinit_rpmsg())), "error! creation of an endpoint failed");

    // send command
    // send message "abc" and get back message "abc"
    data_send_param.dest_addr = my_ept->addr;
    data_send_param.ept_to_ack_addr = ctrl_ept->addr;
    env_memcpy((void *)data_send_param.msg, (void *)data, 4);
    data_send_param.msg_size = 4;
    control_msg.CMD = CTR_CMD_SEND;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_send_param), sizeof(struct control_message_data_send_param));
    result = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&control_msg,
                             sizeof(struct control_message), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for acknowledge message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_SEND ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of send command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! send operation in the other side failed");
    /* wait for responder's message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((0 == env_strncmp(trans_data, data, 3) ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! incorrect data received");

    // send an empty message "" and get back "cba"
    control_msg.ACK_REQUIRED = ACK_REQUIRED_NO;
    data_send_param.msg[0] = '\0';
    data_send_param.msg_size = 1;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_send_param), sizeof(struct control_message_data_send_param));
    result = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&control_msg,
                             sizeof(struct control_message), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for responder's message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((0 == env_strncmp(trans_data, "cba", 3) ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! incorrect data received");

    // send nocopy command
    // send message "abc nocopy" and get back message "abc nocopy"
    data_send_param.dest_addr = my_ept->addr;
    data_send_param.ept_to_ack_addr = ctrl_ept->addr;
    env_memcpy((void *)data_send_param.msg, (void *)data_2, 11);
    data_send_param.msg_size = 11;
    control_msg.CMD = CTR_CMD_SEND_NO_COPY;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_send_param), sizeof(struct control_message_data_send_param));
    result = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&control_msg,
                             sizeof(struct control_message), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for acknowledge message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_SEND_NO_COPY ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of send nocopy command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! send operation in the other side failed");
    /* wait for responder's message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((0 == env_strncmp(trans_data, data, 3) ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! incorrect data received");

    // send an empty message "" and get back "cba nocopy"
    control_msg.ACK_REQUIRED = ACK_REQUIRED_NO;
    data_send_param.msg[0] = '\0';
    data_send_param.msg_size = 1;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_send_param), sizeof(struct control_message_data_send_param));
    result = rpmsg_lite_send(my_rpmsg, ctrl_ept, remote_addr, (char *)&control_msg,
                             sizeof(struct control_message), RL_BLOCK);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for responder's message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((0 == env_strncmp(trans_data, "cba nocopy", 10) ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! incorrect data received");

    rpmsg_lite_destroy_ept(my_rpmsg, my_ept);
    env_delete_mutex(mutex);
end:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "system clean up error");
}

void run_tests(void *unused)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("04_ping_pong");
    __coveragescanner_install("04_ping_pong.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(tc_1_create_delete_ep_cmd_sender, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_EXAMPLE(tc_2_send_cmd_sender, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
}

/* EOF */
