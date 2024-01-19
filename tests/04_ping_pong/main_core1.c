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

#include "fsl_common.h"
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
#include "fsl_memory.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
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
CONTROL_MESSAGE control_msg = {0};
uint32_t trans_data = 0;

/* This is for the responder side */
volatile int32_t message_received = 0;
volatile int32_t message_received_2 = 0;
ACKNOWLEDGE_MESSAGE ack_msg = {0};
struct rpmsg_lite_endpoint *ctrl_ept = {0};
struct rpmsg_lite_ept_static_context ctrl_ept_ctxt;
struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
struct rpmsg_lite_instance rpmsg_ctxt;
rpmsg_ns_handle ns_handle = NULL;
rpmsg_ns_static_context nameservice_ept_ctxt;

static void app_nameservice_isr_cb(uint32_t new_ept, const char *new_ept_name, uint32_t flags, void *user_data)
{
}


//copied from the rpsmg_lite.c (internal function)
static struct llist *test_rpmsg_lite_get_endpoint_from_addr(struct rpmsg_lite_instance *rpmsg_lite_dev, uint32_t addr)
{
    struct llist *rl_ept_lut_head;

    rl_ept_lut_head = rpmsg_lite_dev->rl_endpoints;
    while (rl_ept_lut_head)
    {
        struct rpmsg_lite_endpoint *rl_ept = (struct rpmsg_lite_endpoint *)rl_ept_lut_head->data;
        if (rl_ept->addr == addr)
        {
            return rl_ept_lut_head;
        }
        rl_ept_lut_head = rl_ept_lut_head->next;
    }
    return RL_NULL;
}

// control ept callback
int32_t test_read_cb(void *payload, uint32_t payload_len, uint32_t src, void *priv)
{
    TEST_ASSERT_MESSAGE(0 == message_received, "interrupt missed");

    env_memcpy((void *)&control_msg, payload, payload_len);
    message_received = 1;

    return RL_RELEASE;
}

// custom ept callback
int32_t ept_cb(void *payload, uint32_t payload_len, uint32_t src, void *priv)
{
    TEST_ASSERT_MESSAGE(0 == message_received_2, "interrupt missed");
    env_memcpy((void *)&trans_data, payload, payload_len);
    message_received_2 = 1;

    return RL_RELEASE;
}

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
    rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);
    
    /* wait for a while to allow the primary side to bind_ns and register the NS callback */
    env_sleep_msec(200);

    ctrl_ept = rpmsg_lite_create_ept(my_rpmsg, TC_LOCAL_EPT_ADDR, test_read_cb, NULL, &ctrl_ept_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ctrl_ept, "'rpmsg_lite_create_ept' failed");

    return 0;
}

// utility: deinitialize rpmsg and environment
int32_t ts_deinit_rpmsg(void)
{
    rpmsg_lite_destroy_ept(my_rpmsg, ctrl_ept);
    rpmsg_lite_deinit(my_rpmsg);
    ctrl_ept = NULL;
    my_rpmsg = NULL;
    return 0;
}

// this test case is to test the endpoint functionality.
void tc_1_create_delete_ep_cmd_responder(void)
{
    void *mutex = NULL;
    int32_t result = 0;
    struct rpmsg_lite_endpoint *my_ept, *ept_to_delete;
    struct rpmsg_lite_ept_static_context my_ept_ctxt;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM_PTR data_create_ep_param;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM_PTR data_destroy_ep_param;
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to init");
    if (result)
        goto end;

    // invalid params for rpmsg_ns_bind
    ns_handle = rpmsg_ns_bind(my_rpmsg, RL_NULL, ((void *)0), &nameservice_ept_ctxt);
    TEST_ASSERT_MESSAGE(RL_NULL == ns_handle, "'rpmsg_ns_bind' with bad app_cb param failed");
    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, ((void *)0), RL_NULL);
    TEST_ASSERT_MESSAGE(RL_NULL == ns_handle, "'rpmsg_ns_bind' with bad ns_ept_ctxt param failed");
    
    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, ((void *)0), &nameservice_ept_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ns_handle, "'rpmsg_ns_bind' failed");
    
    // invalid params for rpmsg_ns_announce
    result = rpmsg_ns_announce(my_rpmsg, ctrl_ept, RL_NULL, (uint32_t)RL_NS_CREATE);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_ns_announce' with bad ept_name param failed");
    result = rpmsg_ns_announce(my_rpmsg, RL_NULL, RPMSG_LITE_NS_ANNOUNCE_STRING, (uint32_t)RL_NS_CREATE);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_ns_announce' with bad new_ept param failed");

    // send invalid NS message to the RL_NS_EPT_ADDR - wrong paylod_len identified in the ns cb and the message is dropped (condition coverage increase)
    rpmsg_lite_send(my_rpmsg, ctrl_ept, RL_NS_EPT_ADDR, (char *)&ack_msg, sizeof(ACKNOWLEDGE_MESSAGE), RL_BLOCK);
    
    /* wait for a while to allow the primary side to bind_ns and register the NS callback */
    env_sleep_msec(200);
    // send correct NS message
    result = rpmsg_ns_announce(my_rpmsg, ctrl_ept, RPMSG_LITE_NS_ANNOUNCE_STRING, (uint32_t)RL_NS_CREATE);
    TEST_ASSERT_MESSAGE(RL_SUCCESS == result, "'rpmsg_ns_announce' failed");

    result = env_create_mutex(&mutex, 1, NULL);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to create mutex");
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);

    // create an endpoint.
    /* wait for control message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting the create endpoint command");
    data_create_ep_param = env_allocate_memory(sizeof(struct control_message_data_create_ept_param));
    env_memcpy((void *)(data_create_ep_param), (void *)control_msg.DATA,
               sizeof(struct control_message_data_create_ept_param));
    my_ept = rpmsg_lite_create_ept(my_rpmsg, data_create_ep_param->ept_to_create_addr, ept_cb, NULL, &my_ept_ctxt);
    TEST_ASSERT_MESSAGE((NULL != my_ept ? 1 : (0 != ts_deinit_rpmsg())), "error! creation of an endpoint failed");

    /* this is to send back the ack message */
    if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
    {
        env_memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
        ack_msg.CMD_ACK = CTR_CMD_CREATE_EP;
        ack_msg.RETURN_VALUE = (my_ept == NULL);
        ack_msg.RESP_DATA[0] = my_ept->addr;
        result = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_create_ep_param->ept_to_ack_addr, (char *)&ack_msg,
                                 sizeof(struct acknowledge_message), RL_BLOCK);
        TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        TEST_ASSERT_MESSAGE((my_ept != NULL ? 1 : (0 != ts_deinit_rpmsg())), "error! endpoint creation failed");

        /* wait for a message from sender to newly created endpoint */
        while (!message_received_2)
            ;
        TEST_ASSERT_MESSAGE((EP_SIGNATURE == trans_data ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! failed to receive from custom endpoint");
        trans_data = 0;
        env_lock_mutex(mutex);
        message_received_2 = 0;
        env_unlock_mutex(mutex);
    }

    // create another endpoint with same address as above.
    /* wait for control message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting the create endpoint command");
    env_memcpy((void *)(data_create_ep_param), (void *)control_msg.DATA,
               sizeof(struct control_message_data_create_ept_param));
    my_ept = NULL;
    my_ept = rpmsg_lite_create_ept(my_rpmsg, data_create_ep_param->ept_to_create_addr, ept_cb, NULL, &my_ept_ctxt);
    /* send the ack message if required */
    if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
    {
        env_memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
        ack_msg.CMD_ACK = CTR_CMD_CREATE_EP;
        ack_msg.RETURN_VALUE = (my_ept == NULL);
        result = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_create_ep_param->ept_to_ack_addr, (char *)&ack_msg,
                                 sizeof(struct acknowledge_message), RL_BLOCK);
        TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        TEST_ASSERT_MESSAGE((my_ept == NULL ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! creation of endpoint with similar address failed");
    }
    env_free_memory(data_create_ep_param);

    // delete endpoint
    data_destroy_ep_param = env_allocate_memory(sizeof(struct control_message_data_destroy_ept_param));
    for (int32_t i = 0; i < 2; i++)
    {
        /* wait for control message */
        while (!message_received)
            ;
        env_lock_mutex(mutex);
        message_received = 0;
        env_unlock_mutex(mutex);
        TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_DESTROY_EP ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! expecting the destroy endpoint command");
        env_memcpy((void *)(data_destroy_ep_param), (void *)control_msg.DATA,
                   sizeof(struct control_message_data_destroy_ept_param));
        struct llist *node;
        node = test_rpmsg_lite_get_endpoint_from_addr(my_rpmsg, data_destroy_ep_param->ept_to_destroy_addr);
        if (node != NULL)
        {
            ept_to_delete = (struct rpmsg_lite_endpoint *)node->data;
            rpmsg_lite_destroy_ept(my_rpmsg, ept_to_delete);
        }
        /* send the ack message if required */
        if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
        {
            env_memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
            ack_msg.CMD_ACK = CTR_CMD_DESTROY_EP;
            ack_msg.RETURN_VALUE = (node == NULL);
            result = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_destroy_ep_param->ept_to_ack_addr, (char *)&ack_msg,
                                     sizeof(struct acknowledge_message), RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        }
    }
    env_free_memory(data_destroy_ep_param);

    env_delete_mutex(mutex);
end:
    rpmsg_ns_unbind(my_rpmsg, ns_handle);
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "system clean up error");
}

// this test case is to test the send functionality.
void tc_2_send_cmd_responder(void)
{
    void *mutex = NULL, *data_addr = NULL;
    int32_t result = 0;
    uint32_t buf_size;
    CONTROL_MESSAGE_DATA_SEND_PARAM_PTR data_send_param;
    char data[4] = "cba";
    char data_2[11] = "cba nocopy";
    ACKNOWLEDGE_MESSAGE ack_msg = {0};

    result = env_create_mutex(&mutex, 1, NULL);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to create mutex");

    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);

    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to init");
    if (result)
        goto end;

    // send NS message
    result = rpmsg_ns_announce(my_rpmsg, ctrl_ept, RPMSG_LITE_NS_ANNOUNCE_STRING, (uint32_t)RL_NS_CREATE);
    TEST_ASSERT_MESSAGE(RL_SUCCESS == result, "'rpmsg_ns_announce' failed");

    data_send_param = env_allocate_memory(sizeof(struct control_message_data_send_param));
    // execute the send command
    for (int32_t i = 0; i < 2; i++)
    {
        /* wait for control message */
        while (!message_received)
            ;
        env_lock_mutex(mutex);
        message_received = 0;
        env_unlock_mutex(mutex);
        TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_SEND ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! expecting the send command");
        env_memcpy((void *)(data_send_param), (void *)control_msg.DATA, sizeof(struct control_message_data_send_param));
        if (env_strncmp(data_send_param->msg, "", 1) == 0)
        {
            result = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_send_param->dest_addr, data, 4, RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
        }
        else
        {
            result = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_send_param->dest_addr, data_send_param->msg,
                                     data_send_param->msg_size, RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
        }
        /* send the ack message if required */
        if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
        {
            env_memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
            ack_msg.CMD_ACK = CTR_CMD_SEND;
            ack_msg.RETURN_VALUE = result;
            result = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_send_param->ept_to_ack_addr, (char *)&ack_msg,
                                     sizeof(struct acknowledge_message), RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        }
    }

    // execute the send nocopy command
    for (int32_t i = 0; i < 2; i++)
    {
        /* wait for control message */
        while (!message_received)
            ;
        env_lock_mutex(mutex);
        message_received = 0;
        env_unlock_mutex(mutex);
        TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_SEND_NO_COPY ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! expecting the send nocopy command");
        env_memcpy((void *)(data_send_param), (void *)control_msg.DATA, sizeof(struct control_message_data_send_param));
        data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_BLOCK);
        TEST_ASSERT_MESSAGE((NULL != data_addr ? 1 : (0 != ts_deinit_rpmsg())), "error! no more buffer available");
        if (env_strncmp(data_send_param->msg, "", 1) == 0)
        {
            env_memcpy(data_addr, data_2, 11);
            result = rpmsg_lite_send_nocopy(my_rpmsg, ctrl_ept, data_send_param->dest_addr, data_addr, 11);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
            data_addr = NULL;
        }
        else
        {
            env_memcpy(data_addr, data_send_param->msg, data_send_param->msg_size);
            result = rpmsg_lite_send_nocopy(my_rpmsg, ctrl_ept, data_send_param->dest_addr, data_addr,
                                            data_send_param->msg_size);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
            data_addr = NULL;
        }
        /* send the ack message if required */
        if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
        {
            env_memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
            ack_msg.CMD_ACK = CTR_CMD_SEND_NO_COPY;
            ack_msg.RETURN_VALUE = result;
            result = rpmsg_lite_send(my_rpmsg, ctrl_ept, data_send_param->ept_to_ack_addr, (char *)&ack_msg,
                                     sizeof(struct acknowledge_message), RL_BLOCK);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        }
    }

    env_free_memory(data_send_param);
    env_delete_mutex(mutex);
end:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "system clean up error");
}

void run_tests(void *unused)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("04_ping_pong_sec_core");
    __coveragescanner_install("04_ping_pong_sec_core.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(tc_1_create_delete_ep_cmd_responder, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_EXAMPLE(tc_2_send_cmd_responder, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
}

/* EOF */
