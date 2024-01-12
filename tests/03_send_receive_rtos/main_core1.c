/*
 * Copyright 2016-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rpmsg_lite.h"
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#include "assert.h"
#include "app.h"
#if defined(SDK_OS_FREE_RTOS)
#include "FreeRTOS.h"
#include "task.h"
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

struct rpmsg_lite_endpoint *volatile my_ept = NULL;
rpmsg_queue_handle my_queue = NULL;
struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
rpmsg_ns_handle ns_handle = NULL;

static void app_nameservice_isr_cb(uint32_t new_ept, const char *new_ept_name, uint32_t flags, void *user_data)
{
}

// utility: initialize rpmsg and environment
// and wait for default channel
int32_t ts_init_rpmsg(void)
{
    env_init();
#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
    my_rpmsg = rpmsg_lite_remote_init(rpmsg_lite_base, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
#else
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
    my_rpmsg = rpmsg_lite_remote_init((void *)MEMORY_ConvertMemoryMapAddress((uint32_t)RPMSG_LITE_SHMEM_BASE, kMEMORY_DMA2Local), RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
#else
    my_rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
#endif
#endif /* SH_MEM_NOT_TAKEN_FROM_LINKER */
    TEST_ASSERT_MESSAGE(NULL != my_rpmsg, "init function failed");
    rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);
    
    /* wait for a while to allow the primary side to bind_ns and register the NS callback */
    env_sleep_msec(200);

    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, ((void *)0));
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
int32_t ts_create_epts(rpmsg_queue_handle queues[], struct rpmsg_lite_endpoint *volatile epts[], int32_t count, int32_t init_addr)
{
    TEST_ASSERT_MESSAGE(queues != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");

    // invalid params for rpmsg_queue_create
    TEST_ASSERT_MESSAGE(RL_NULL == rpmsg_queue_create(RL_NULL), "'rpmsg_queue_create' with bad rpmsg_lite_dev param failed");
#if !(defined(CPU_MIMXRT685SFAWBR_dsp) || defined(CPU_MIMXRT685SFFOB_dsp) || defined(CPU_MIMXRT685SFVKB_dsp) || \
     defined(CPU_MIMXRT595SFAWC_dsp) || defined(CPU_MIMXRT595SFFOC_dsp))
    //force env_create_queue() call inside rpmsg_queue_create() to fail - request to allocate too much memory
    uint16_t temp = my_rpmsg->rvq->vq_nentries;
    my_rpmsg->rvq->vq_nentries = 0xfffe;
    TEST_ASSERT_MESSAGE(RL_NULL == rpmsg_queue_create(my_rpmsg), "'rpmsg_queue_create' with forced error in queue allocation failed");
    my_rpmsg->rvq->vq_nentries = temp;
#endif
    for (int32_t i = 0; i < count; i++)
    {
        queues[i] = rpmsg_queue_create(my_rpmsg);
        epts[i] = rpmsg_lite_create_ept(my_rpmsg, init_addr + i, rpmsg_queue_rx_cb, queues[i]);
        TEST_ASSERT_MESSAGE(NULL != epts[i], "'rpmsg_lite_create_ept' failed");
        TEST_ASSERT_MESSAGE(init_addr + i == epts[i]->addr,
                            "'rpmsg_lite_create_ept' does not provide expected address");
    }
    return 0;
}

// utility: destroy number of epts
int32_t ts_destroy_epts(rpmsg_queue_handle queues[], struct rpmsg_lite_endpoint *volatile epts[], int32_t count)
{
    TEST_ASSERT_MESSAGE(queues != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");

    // invalid params for rpmsg_queue_destroy
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == rpmsg_queue_destroy(RL_NULL, queues[0]), "'rpmsg_queue_destroy' with bad rpmsg_lite_dev param failed");
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == rpmsg_queue_destroy(my_rpmsg, RL_NULL), "'rpmsg_queue_destroy' with bad q param failed");

    for (int32_t i = 0; i < count; i++)
    {
        rpmsg_queue_destroy(my_rpmsg, queues[i]);
        rpmsg_lite_destroy_ept(my_rpmsg, epts[i]);
    }
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
 * Test case 1
 * - verify simple transport between default epts of default channels
 * - verify simple nocopy transport between default epts of default channels
 * - verify simple transport between custom created epts of default channels
 * - verify simple nocopy transport between custom created epts of default channels
 *****************************************************************************/
void tc_1_receive_send(void)
{
    int32_t result = 0;
    char data[DATA_LEN] = {0};
    void *data_addr = NULL;
    uint32_t src;
    uint32_t len;
#ifdef __COVERAGESCANNER__
    uint16_t my_rpmsg_hdr_idx = 0;
    struct my_rpmsg_std_msg *msg;
#endif /*__COVERAGESCANNER__*/

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        result = rpmsg_queue_recv(my_rpmsg, my_queue, &src, data, DATA_LEN, &len, RL_BLOCK);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = pattern_cmp(data, i, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        result = rpmsg_queue_recv_nocopy(my_rpmsg, my_queue, &src, (char **)&data_addr, &len, RL_BLOCK);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = pattern_cmp(data_addr, i, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");

#ifdef __COVERAGESCANNER__
        /* Force ERROR_VRING_NO_BUFF error in virtqueue_add_consumed_buffer() when RL_ASSERT is off in Coco tests */
        msg = (struct my_rpmsg_std_msg *)(void *)((char *)(data_addr)-offsetof(struct my_rpmsg_std_msg, data));
        my_rpmsg_hdr_idx = msg->hdr.reserved.idx;
        msg->hdr.reserved.idx = 2*RL_BUFFER_COUNT;
        result = rpmsg_queue_nocopy_free(my_rpmsg, data_addr);
        msg->hdr.reserved.idx = my_rpmsg_hdr_idx;
#endif /*__COVERAGESCANNER__*/

        result = rpmsg_queue_nocopy_free(my_rpmsg, data_addr);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    /* for invalid length remote receive */
    result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 == result, "negative number");

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        env_memset(data, i, DATA_LEN);
        result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_BLOCK);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        env_memset(data, i, DATA_LEN);
        result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_BLOCK);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    // send message to non-existing endpoint address, message will be dropped on the receiver side
    result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR + 1, data, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 == result, "negative number");

    // send - invalid rpmsg_lite_dev
    result = rpmsg_lite_send(RL_NULL, my_ept, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_lite_send' with bad rpmsg_lite_dev param failed");

    // invalid params for send
    result = rpmsg_lite_send(my_rpmsg, NULL, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, NULL, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, 0xFFFFFFFF, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");

    // invalid params for receive
    result = rpmsg_queue_recv(RL_NULL, my_queue, &src, data, DATA_LEN, &len, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_recv' with bad rpmsg_lite_dev param failed");
    result = rpmsg_queue_recv(my_rpmsg, RL_NULL, &src, data, DATA_LEN, &len, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_recv' with bad q param failed");
    result = rpmsg_queue_recv(my_rpmsg, my_queue, &src, RL_NULL, DATA_LEN, &len, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_recv' with bad data param failed");
    result = rpmsg_queue_recv(my_rpmsg, my_queue, &src, data, 0, &len, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_BUFF_SIZE == result, "'rpmsg_queue_recv' with bad maxlen param failed");

    // invalid params for receive_nocopy
    result = rpmsg_queue_recv_nocopy(RL_NULL, my_queue, &src, (char **)&data_addr, &len, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_recv_nocopy' with bad rpmsg_lite_dev param failed");
    result = rpmsg_queue_recv_nocopy(my_rpmsg, RL_NULL, &src, (char **)&data_addr, &len, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_recv_nocopy' with bad q param failed");
    result = rpmsg_queue_recv_nocopy(my_rpmsg, my_queue, &src, RL_NULL, &len, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_recv_nocopy' with bad data param failed");

    // invalid params for receive_nocopy_free
    result = rpmsg_queue_nocopy_free(RL_NULL, data_addr);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_nocopy_free' with bad rpmsg_lite_dev param failed");
    result = rpmsg_queue_nocopy_free(my_rpmsg, RL_NULL);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_nocopy_free' with bad data param failed");

    // invalid params for rpmsg_lite_release_rx_buffer - this should not be used in an app and rpmsg_queue_nocopy_free should be used instead
    result = rpmsg_lite_release_rx_buffer(RL_NULL, data_addr);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_lite_release_rx_buffer' with bad rpmsg_lite_dev param failed");
    result = rpmsg_lite_release_rx_buffer(my_rpmsg, RL_NULL);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_lite_release_rx_buffer' with bad data param failed");
}

/******************************************************************************
 * Test case 2
 * - same as for test case 1 but send with copy replaced by send_nocopy
 *****************************************************************************/
void tc_2_receive_send(void)
{
    int32_t result = 0;
    char data[DATA_LEN] = {0};
    void *data_addr = NULL;
    uint32_t buf_size = 0;
    uint32_t src;
    uint32_t len;

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        result = rpmsg_queue_recv(my_rpmsg, my_queue, &src, data, DATA_LEN, &len, RL_BLOCK);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = pattern_cmp(data, i, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        result = rpmsg_queue_recv_nocopy(my_rpmsg, my_queue, &src, (char **)&data_addr, &len, RL_BLOCK);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = pattern_cmp(data_addr, i, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = rpmsg_queue_nocopy_free(my_rpmsg, data_addr);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_BLOCK);
        TEST_ASSERT_MESSAGE(NULL != data_addr, "negative number");
        TEST_ASSERT_MESSAGE(0 != buf_size, "negative number");
        env_memset(data_addr, i, DATA_LEN);
        result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data_addr, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        data_addr = NULL;
    }

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_BLOCK);
        TEST_ASSERT_MESSAGE(NULL != data_addr, "negative number");
        TEST_ASSERT_MESSAGE(0 != buf_size, "negative number");
        env_memset(data_addr, i, DATA_LEN);
        result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data_addr, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        data_addr = NULL;
    }

    // invalid params for alloc_tx_buffer
    data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, NULL, RL_BLOCK);
    TEST_ASSERT_MESSAGE(NULL == data_addr, "negative number");

    // invalid params for send_nocopy
    result = rpmsg_lite_send_nocopy(my_rpmsg, NULL, TC_REMOTE_EPT_ADDR, data_addr, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, NULL, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data_addr, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");

    // wait until all messages are consumed on the primary side and a new message is received
    result = rpmsg_queue_recv(my_rpmsg, my_queue, &src, data, DATA_LEN, &len, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    result = pattern_cmp(data, 0, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    // send RL_BUFFER_COUNT messages to made the message queue on the primary side full
#if !(defined(RL_ALLOW_CUSTOM_SHMEM_CONFIG) && (RL_ALLOW_CUSTOM_SHMEM_CONFIG == 1))
    for (int32_t i = 0; i < RL_BUFFER_COUNT; i++)
#else
    for (int32_t i = 0; i < RL_BUFFER_COUNT(RPMSG_LITE_LINK_ID); i++)
#endif
    {
        data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_BLOCK);
        TEST_ASSERT_MESSAGE(NULL != data_addr, "negative number");
        TEST_ASSERT_MESSAGE(0 != buf_size, "negative number");
        env_memset(data_addr, i, DATA_LEN);
        result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data_addr, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        data_addr = NULL;
    }
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
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    result = ts_create_epts(&my_queue, &my_ept, 1, TC_LOCAL_EPT_ADDR);
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    TEST_ASSERT_MESSAGE(RL_SUCCESS == rpmsg_ns_announce(my_rpmsg, my_ept, RPMSG_LITE_NS_ANNOUNCE_STRING, (uint32_t)RL_NS_CREATE), "'rpmsg_ns_announce' failed");

    if (!result)
    {
#ifdef __COVERAGESCANNER__
        __coveragescanner_testname("03_send_receive_rtos_sec_core");
        __coveragescanner_install("03_send_receive_rtos_sec_core.csexe");
#endif /*__COVERAGESCANNER__*/
        RUN_EXAMPLE(tc_1_receive_send, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
        RUN_EXAMPLE(tc_2_receive_send, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    }
    ts_destroy_epts(&my_queue, &my_ept, 1);
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
}
