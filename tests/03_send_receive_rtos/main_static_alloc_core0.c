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
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TC_TRANSFER_COUNT 10
#define DATA_LEN 45
#define TC_LOCAL_EPT_ADDR (40)
#define TC_REMOTE_EPT_ADDR (30)

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
    uint32_t src;
    void *data;
    uint32_t len;
} my_rpmsg_queue_rx_cb_data_t;

/*******************************************************************************
 * Code
 ******************************************************************************/
struct rpmsg_lite_endpoint *volatile my_ept = NULL;
struct rpmsg_lite_ept_static_context my_ept_ctxt;

rpmsg_queue_handle my_queue = NULL;
rpmsg_static_queue_ctxt my_queue_ctxt = {0};
uint8_t my_rpmsg_queue_storage[RL_ENV_QUEUE_STATIC_STORAGE_SIZE] = {0};
struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
struct rpmsg_lite_instance rpmsg_ctxt = {0};
rpmsg_ns_handle ns_handle = NULL;
rpmsg_ns_static_context my_ns_ctxt = {0};
volatile uint32_t remote_addr = 0U;
void *aux_mutex = NULL;
LOCK_STATIC_CONTEXT aux_mutex_ctxt = {0};
void *aux_q = RL_NULL;
rpmsg_static_queue_ctxt aux_queue_ctxt = {0};
uint8_t aux_rpmsg_queue_storage[RL_ENV_QUEUE_STATIC_STORAGE_SIZE];

static void app_nameservice_isr_cb(uint32_t new_ept, const char *new_ept_name, uint32_t flags, void *user_data)
{
    uint32_t *data = (uint32_t *)user_data;

    *data = new_ept;

    //test mutex-related operations in ISR
    env_lock_mutex(aux_mutex);
    env_unlock_mutex(aux_mutex);
    //test env_get_timestamp() being called from ISR
    TEST_ASSERT_MESSAGE(0 < env_get_timestamp(), "env_get_timestamp function in ISR failed");
    //test queue-related operations in ISR
    TEST_ASSERT_MESSAGE(0 == env_get_current_queue_size((void *)aux_q), "env_get_current_queue_size function in ISR failed");
}

// utility: initialize rpmsg and environment
// and wait for default channel
int32_t ts_init_rpmsg(void)
{
    env_init();
    env_sleep_msec(200);
#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
    my_rpmsg = rpmsg_lite_master_init(rpmsg_lite_base, SH_MEM_TOTAL_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS, &rpmsg_ctxt);
#else
    my_rpmsg = rpmsg_lite_master_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_SHMEM_SIZE, RPMSG_LITE_LINK_ID,
                                      RL_NO_FLAGS, &rpmsg_ctxt);
#endif /* SH_MEM_NOT_TAKEN_FROM_LINKER */
    TEST_ASSERT_MESSAGE(NULL != my_rpmsg, "init function failed");

    ns_handle = rpmsg_ns_bind(my_rpmsg, app_nameservice_isr_cb, (void *)&remote_addr, &my_ns_ctxt);
    TEST_ASSERT_MESSAGE(NULL != ns_handle, "'rpmsg_ns_bind' failed");
    return 0;
}

// utility: deinitialize rpmsg and environment
int32_t ts_deinit_rpmsg(void)
{
    rpmsg_ns_unbind(my_rpmsg, ns_handle);
    ns_handle = NULL;
    env_memset(&my_ns_ctxt, 0, sizeof(rpmsg_ns_static_context));
    rpmsg_lite_deinit(my_rpmsg);
    my_rpmsg = NULL;
    env_memset(&rpmsg_ctxt, 0, sizeof(struct rpmsg_lite_instance));
    return 0;
}

// utility: create number of epts
int32_t ts_create_epts(rpmsg_queue_handle queues[], uint8_t queues_storage[], rpmsg_static_queue_ctxt queues_ctxt[], struct rpmsg_lite_endpoint *volatile epts[], struct rpmsg_lite_ept_static_context epts_ctxt[], int32_t count, int32_t init_addr)
{
    TEST_ASSERT_MESSAGE(queues != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");

    // invalid params for rpmsg_queue_create
    TEST_ASSERT_MESSAGE(RL_NULL == rpmsg_queue_create(RL_NULL, &queues_storage[0], &queues_ctxt[0]), "'rpmsg_queue_create' with bad rpmsg_lite_dev param failed");
    TEST_ASSERT_MESSAGE(RL_NULL == rpmsg_queue_create(my_rpmsg, RL_NULL, &queues_ctxt[0]), "'rpmsg_queue_create' with bad rpmsg_lite_dev param failed");
    TEST_ASSERT_MESSAGE(RL_NULL == rpmsg_queue_create(my_rpmsg, &queues_storage[0], RL_NULL), "'rpmsg_queue_create' with bad rpmsg_lite_dev param failed");

    for (int32_t i = 0; i < count; i++)
    {
        queues[i] = rpmsg_queue_create(my_rpmsg, &queues_storage[i], &queues_ctxt[i]);
        epts[i] = rpmsg_lite_create_ept(my_rpmsg, init_addr + i, rpmsg_queue_rx_cb, queues[i], &epts_ctxt[i]);
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
 * - verify simple transport between custom created epts
 * - verify simple nocopy transport between custom created epts
 *****************************************************************************/
void tc_1_send_receive(void)
{
    int32_t result = 0;
    char data[DATA_LEN] = {0};
    void *data_addr = NULL;
    uint32_t src;
    uint32_t len;

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        env_memset(data, i, DATA_LEN);
        while(RL_SUCCESS != rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, DATA_LEN, 1));
    }

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        env_memset(data, i, DATA_LEN);
        while(RL_SUCCESS != rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_DONT_BLOCK));
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

    // invalid params for rpmsg_queue_get_current_size
    result = rpmsg_queue_get_current_size(RL_NULL);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_get_current_size' with bad q param failed");

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

    /* for invalid length remote receive */
    result = rpmsg_lite_send(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, DATA_LEN, RL_BLOCK);
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
}

/******************************************************************************
 * Test case 2
 * - same as for test case 1 but send with copy replaced by send_nocopy
 *****************************************************************************/
void tc_2_send_receive(void)
{
    int32_t result = 0;
    char data[DATA_LEN] = {0};
    void *data_addr = NULL;
    uint32_t buf_size = 0;
    uint32_t src;
    uint32_t len;
    my_rpmsg_queue_rx_cb_data_t fake_msg = {0};

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, 1);
        while(RL_NULL == data_addr)
        {
            data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, 1);
        }
        TEST_ASSERT_MESSAGE(NULL != data_addr, "negative number");
        TEST_ASSERT_MESSAGE(0 != buf_size, "negative number");
        env_memset(data_addr, i, DATA_LEN);
        result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data_addr, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        data_addr = NULL;
    }

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_DONT_BLOCK);
        while(RL_NULL == data_addr)
        {
            data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_DONT_BLOCK);
        }
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
    result = rpmsg_lite_send_nocopy(my_rpmsg, NULL, TC_REMOTE_EPT_ADDR, data, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, NULL, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");

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
    
    // put fake message into the queue to allow queue full state and incomming messages dropping
    TEST_ASSERT_MESSAGE(1 == env_put_queue(my_queue, &fake_msg, 0), "env_put_queue function failed");
    // send a message to the secondary side to trigger messages sending from the secondary side to the primary side
    data_addr = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &buf_size, RL_BLOCK);
    TEST_ASSERT_MESSAGE(NULL != data_addr, "negative number");
    TEST_ASSERT_MESSAGE(0 != buf_size, "negative number");
    env_memset(data_addr, 0, DATA_LEN);
    result = rpmsg_lite_send_nocopy(my_rpmsg, my_ept, TC_REMOTE_EPT_ADDR, data_addr, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    data_addr = NULL;
    //wait a while to allow the secondary side to send all messages and made the receive queue full
    env_sleep_msec(200);
    // invalid src and len pointer params for receive
    result = rpmsg_queue_recv(my_rpmsg, my_queue, RL_NULL, data, DATA_LEN, RL_NULL, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "'rpmsg_queue_recv' with bad src and len pointer param failed");
    result = rpmsg_queue_recv_nocopy(my_rpmsg, my_queue, RL_NULL, (char **)&data_addr, RL_NULL, RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_SUCCESS == result, "'rpmsg_queue_recv_nocopy' with bad bad src and len pointer param failed");
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

    TEST_ASSERT_MESSAGE(0 == env_create_mutex(&aux_mutex, 1, &aux_mutex_ctxt), "env_create_mutex function failed");
    TEST_ASSERT_MESSAGE(0 == env_create_queue(&aux_q, 1, sizeof(uint32_t), &aux_rpmsg_queue_storage[0], &aux_queue_ctxt), "env_create_queue function failed");

    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    result = ts_create_epts(&my_queue, &my_rpmsg_queue_storage[0], &my_queue_ctxt, &my_ept, &my_ept_ctxt, 1, TC_LOCAL_EPT_ADDR);
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    /* Wait until the secondary core application issues the nameservice isr and the remote endpoint address is known. */
    while (0U == remote_addr)
    {
    };
    if (!result)
    {
#ifdef __COVERAGESCANNER__
        __coveragescanner_testname("03_send_receive_rtos");
        __coveragescanner_install("03_send_receive_rtos.csexe");
#endif /*__COVERAGESCANNER__*/
        RUN_EXAMPLE(tc_1_send_receive, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
        RUN_EXAMPLE(tc_2_send_receive, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    }
    env_delete_mutex(aux_mutex);    
    env_memset(&aux_mutex_ctxt, 0, sizeof(LOCK_STATIC_CONTEXT));
    env_delete_queue(aux_q);
    env_memset(&aux_rpmsg_queue_storage, 0, RL_ENV_QUEUE_STATIC_STORAGE_SIZE);
    env_memset(&aux_queue_ctxt, 0, sizeof(rpmsg_static_queue_ctxt));

    ts_destroy_epts(&my_queue, &my_ept, 1);
    my_queue = NULL;
    env_memset(&my_rpmsg_queue_storage, 0, RL_ENV_QUEUE_STATIC_STORAGE_SIZE);
    env_memset(&my_queue_ctxt, 0, sizeof(rpmsg_static_queue_ctxt));
    my_ept = NULL;
    env_memset(&my_ept_ctxt, 0, sizeof(struct rpmsg_lite_ept_static_context));
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
}
