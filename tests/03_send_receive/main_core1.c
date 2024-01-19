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

#include "fsl_common.h"
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
#include "fsl_memory.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define DATA_LEN 45
#define TRYSEND_COUNT 12
#define TC_LOCAL_EPT_ADDR (30)
#define TC_REMOTE_EPT_ADDR (40)
#define RPMSG_LITE_NS_ANNOUNCE_STRING "rpmsg-test-channel"

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
#ifdef __COVERAGESCANNER__
/* rpmsg_std_hdr contains a reserved field,
 * this implementation of RPMSG uses this reserved
 * field to hold the idx and totlen of the buffer
 * not being returned to the vring in the receive
 * callback function. This way, the no-copy API
 * can use this field to return the buffer later.
 */
struct my_rpmsg_hdr_reserved
{
    uint16_t rfu; /* reserved for future usage */
    uint16_t idx;
};

RL_PACKED_BEGIN
/*!
 * Common header for all rpmsg messages.
 * Every message sent/received on the rpmsg bus begins with this header.
 */
struct my_rpmsg_std_hdr
{
    uint32_t src;                       /*!< source endpoint address */
    uint32_t dst;                       /*!< destination endpoint address */
    struct my_rpmsg_hdr_reserved reserved; /*!< reserved for future use */
    uint16_t len;                       /*!< length of payload (in bytes) */
    uint16_t flags;                     /*!< message flags */
} RL_PACKED_END;

RL_PACKED_BEGIN
/*!
 * Common message structure.
 * Contains the header and the payload.
 */
struct my_rpmsg_std_msg
{
    struct my_rpmsg_std_hdr hdr; /*!< RPMsg message header */
    uint8_t data[1];          /*!< bytes of message payload data */
} RL_PACKED_END;
#endif /*__COVERAGESCANNER__*/

volatile int32_t test_no = 0, rx_data_len = 0;
void *rx_buffer = NULL;
struct rpmsg_lite_endpoint *volatile my_ept = NULL;
struct rpmsg_lite_endpoint *aux_ept = NULL;
struct rpmsg_lite_ept_static_context my_ept_ctxt;
struct rpmsg_lite_ept_static_context aux_ept_ctxt;
struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
struct rpmsg_lite_instance rpmsg_ctxt;
rpmsg_ns_handle ns_handle = NULL;
rpmsg_ns_static_context nameservice_ept_ctxt;

static void app_nameservice_isr_cb(uint32_t new_ept, const char *new_ept_name, uint32_t flags, void *user_data)
{
}

int32_t pattern_cmp(char *buffer, char pattern, int32_t len)
{
    for (int32_t i = 0; i < len; i++)
        if (buffer[i] != pattern)
            return -1;
    return 0;
}

// endpoint callback
int32_t ept_cb(void *data, uint32_t len, uint32_t src, void *priv)
{
    int32_t result = 0;
    TEST_ASSERT_MESSAGE(src == TC_REMOTE_EPT_ADDR, "receive error");
    TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
    switch (test_no)
    {
        case 0:
            result = pattern_cmp(data, test_no, DATA_LEN);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");

            test_no++;
            return RL_RELEASE;

        case 1:
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");

            test_no++;
            return RL_RELEASE;

        case 2:
            rx_data_len = len;
            rx_buffer = data;

            test_no++;
            return RL_HOLD;
    }
    return RL_RELEASE;
}

int32_t aux_ept_cb(void *data, uint32_t len, uint32_t src, void *priv)
{
    /* Introduce some delay and release the message */
    env_sleep_msec(10);
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

    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, ((void *)0), &nameservice_ept_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ns_handle, "'rpmsg_ns_bind' failed");
    return 0;
}

// utility: deinitialize rpmsg and environment
int32_t ts_deinit_rpmsg(void)
{
    rpmsg_ns_unbind(my_rpmsg, ns_handle);
    rpmsg_lite_deinit(my_rpmsg);
    return 0;
}

// utility: create number of epts
int32_t ts_create_epts(struct rpmsg_lite_endpoint *volatile epts[],
                   int32_t count,
                   int32_t init_addr,
                   struct rpmsg_lite_ept_static_context ctxts[])
{
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");
    for (int32_t i = 0; i < count; i++)
    {
        epts[i] = rpmsg_lite_create_ept(my_rpmsg, init_addr + i, ept_cb, NULL, &ctxts[i]);
        TEST_ASSERT_MESSAGE(NULL != epts[i], "'rpmsg_lite_create_ept' failed");
        TEST_ASSERT_MESSAGE(init_addr + i == epts[i]->addr,
                            "'rpmsg_lite_create_ept' does not provide expected address");
    }
    return 0;
}

// utility: destroy number of epts
int32_t ts_destroy_epts(struct rpmsg_lite_endpoint *volatile epts[], int32_t count)
{
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");
    for (int32_t i = 0; i < count; i++)
    {
        rpmsg_lite_destroy_ept(my_rpmsg, epts[i]);
    }
    return 0;
}
/******************************************************************************
 * Test case 1
 * - check received data
 *****************************************************************************/
void tc_1_receive(void)
{
    int32_t result = 0;
#ifdef __COVERAGESCANNER__
    uint16_t my_rpmsg_hdr_idx = 0;
    struct my_rpmsg_std_msg *msg;
#endif /*__COVERAGESCANNER__*/

    // wait for incoming interrupts
    while (test_no != 3)
        ;
    // check the last message content - this message has not been processed within the rx callback
    TEST_ASSERT_MESSAGE(NULL != rx_buffer, "receive error");
    TEST_ASSERT_MESSAGE(0 != rx_data_len, "receive error");
    result = pattern_cmp(rx_buffer, 2, rx_data_len);
    TEST_ASSERT_MESSAGE(0 == result, "receive error");

    /* Before releasing the rx buffer try to call the rpmsg_lite_release_rx_buffer API with incorrect params */
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == rpmsg_lite_release_rx_buffer(RL_NULL, rx_buffer), "'rpmsg_lite_release_rx_buffer' with bad rpmsg_lite_dev param failed");
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == rpmsg_lite_release_rx_buffer(my_rpmsg, RL_NULL), "'rpmsg_lite_release_rx_buffer' with bad rxbuf param failed");

#ifdef __COVERAGESCANNER__
    /* Force ERROR_VRING_NO_BUFF error in virtqueue_add_consumed_buffer() when RL_ASSERT is off in Coco tests */
    msg = (struct my_rpmsg_std_msg *)(void *)((char *)(rx_buffer)-offsetof(struct my_rpmsg_std_msg, data));
    my_rpmsg_hdr_idx = msg->hdr.reserved.idx;
    msg->hdr.reserved.idx = 2*RL_BUFFER_COUNT;
    result = rpmsg_lite_release_rx_buffer(my_rpmsg, rx_buffer);
    msg->hdr.reserved.idx = my_rpmsg_hdr_idx;
#endif /*__COVERAGESCANNER__*/

    /* Release the buffer now */
    result = rpmsg_lite_release_rx_buffer(my_rpmsg, rx_buffer);
    TEST_ASSERT_MESSAGE(RL_SUCCESS == result, "rpmsg_lite_release_rx_buffer error");
}

/******************************************************************************
 * Test case 2
 * - send data with different pattern for each send function
 * - call send function with invalid parameters
 *****************************************************************************/
void tc_2_send(void)
{
    int32_t result = 0;
    char data[DATA_LEN] = {0};
    void *data_addr = NULL;
    uint32_t buf_size = 0;

    // send - invalid rpmsg_lite_dev
    result = rpmsg_lite_send(RL_NULL, my_ept, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_lite_send' with bad rpmsg_lite_dev param failed");

    // send - invalid endpoint
    result = rpmsg_lite_send(my_rpmsg, NULL, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_lite_send_nocopy(my_rpmsg, NULL, TC_REMOTE_EPT_ADDR, data, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");

    // send - invalid data
    result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, NULL, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, NULL, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");

    // send - invalid size
    result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, 0xffffffff, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, 0xffffffff);
    TEST_ASSERT_MESSAGE(0 != result, "send error");

    // invalid params for alloc_tx_buffer
    data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, NULL, RL_BLOCK);
    TEST_ASSERT_MESSAGE(NULL == data_addr, "send error");

    // send - valid all params
    env_memset(data, 0, DATA_LEN);
    result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 == result, "send error");

    // send message to non-existing endpoint address, message will be dropped on the receiver side
    result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR + 1, data, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 == result, "negative number");

    data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 != buf_size, "send error");
    env_memset(data_addr, 1, DATA_LEN);
    result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data_addr, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 == result, "send error");
    data_addr = NULL;

    data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 != buf_size, "send error");
    env_memset(data_addr, 2, DATA_LEN);
    result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data_addr, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 == result, "send error");
    data_addr = NULL;

    /* wait a while to process the last message on the opposite side */
    env_sleep_msec(1000);
}

void run_tests(void *unused)
{
    int32_t result = 0;
    struct rpmsg_lite_endpoint fake_ept = {0};
    struct rpmsg_lite_instance fake_rpmsg = {0};
    uint32_t bufer_size = 1;

    // call send before rpmsg_lite is initialized
    result = rpmsg_lite_send(&fake_rpmsg, &fake_ept, TC_REMOTE_EPT_ADDR, (char*)0x12345678, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_NOT_READY == result, "'rpmsg_lite_send' called before rpmsg_lite is initialized failed");
    TEST_ASSERT_MESSAGE(RL_NULL == rpmsg_lite_alloc_tx_buffer(&fake_rpmsg, &bufer_size, RL_BLOCK), "'rpmsg_lite_alloc_tx_buffer' called before rpmsg_lite is initialized failed");
    TEST_ASSERT_MESSAGE(0 == bufer_size, "'rpmsg_lite_alloc_tx_buffer' called before rpmsg_lite is initialized failed");
    result = rpmsg_lite_send_nocopy(&fake_rpmsg, &fake_ept, TC_REMOTE_EPT_ADDR, (char*)0x12345678, DATA_LEN);
    TEST_ASSERT_MESSAGE(RL_NOT_READY == result, "'rpmsg_lite_send_nocopy' called before rpmsg_lite is initialized failed");

    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "receive error");

    // create custom epts of default channel, check nonrecoverable error
    result = ts_create_epts(&my_ept, 1, TC_LOCAL_EPT_ADDR, &my_ept_ctxt);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_create_epts' failed");

    // create auxiliary endpoint that does not consume messages
    aux_ept = rpmsg_lite_create_ept(my_rpmsg, TC_LOCAL_EPT_ADDR+2, aux_ept_cb, NULL, &aux_ept_ctxt);

    TEST_ASSERT_MESSAGE(RL_SUCCESS == rpmsg_ns_announce(my_rpmsg, my_ept, RPMSG_LITE_NS_ANNOUNCE_STRING, (uint32_t)RL_NS_CREATE), "'rpmsg_ns_announce' failed");

    if (!result)
    {
#ifdef __COVERAGESCANNER__
        __coveragescanner_testname("03_send_receive_sec_core");
        __coveragescanner_install("03_send_receive_sec_core.csexe");
#endif /*__COVERAGESCANNER__*/
        RUN_EXAMPLE(tc_1_receive, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
        RUN_EXAMPLE(tc_2_send, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    }
    result = ts_destroy_epts(&my_ept, 1);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_destroy_epts' failed");
    
    rpmsg_lite_destroy_ept(my_rpmsg, aux_ept);

    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "receive error");
}
