/*
 * Copyright 2016-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rpmsg_lite.h"
#include "unity.h"
#include "rpmsg_queue.h"
#include "assert.h"
#include "app.h"
#if defined(SDK_OS_FREE_RTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TC_INIT_COUNT 10
#define TC_TRANSFER_COUNT 10
#define TC_EPT_COUNT 3
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

/*******************************************************************************
 * Code
 ******************************************************************************/
struct rpmsg_lite_endpoint *default_ept;
struct rpmsg_lite_ept_static_context default_ept_ctxt;

rpmsg_queue_handle default_queue = NULL;
rpmsg_static_queue_ctxt default_queue_ctxt = {0};
uint8_t default_rpmsg_queue_storage[RL_ENV_QUEUE_STATIC_STORAGE_SIZE] = {0};

struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
struct rpmsg_lite_instance rpmsg_ctxt = {0};

// utility: initialize rpmsg and environment
// and wait for default channel
int32_t ts_init_rpmsg(void)
{
    env_init();
    env_sleep_msec(200);
#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
    my_rpmsg = rpmsg_lite_master_init(rpmsg_lite_base, SH_MEM_TOTAL_SIZE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#else
    my_rpmsg = rpmsg_lite_master_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_SHMEM_SIZE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#endif /* SH_MEM_NOT_TAKEN_FROM_LINKER */
    TEST_ASSERT_MESSAGE(NULL != my_rpmsg, "init function failed");
    rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);
    return 0;
}

// utility: deinitialize rpmsg and environment
int32_t ts_deinit_rpmsg(void)
{
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
    for (int32_t i = 0; i < count; i++)
    {
        queues[i] = rpmsg_queue_create(my_rpmsg, &queues_storage[i], &queues_ctxt[i]);
        epts[i] = rpmsg_lite_create_ept(my_rpmsg, init_addr + i, rpmsg_queue_rx_cb, queues[i], &epts_ctxt[i]);
        TEST_ASSERT_MESSAGE(NULL != epts[i], "'rpmsg_lite_create_ept' failed");
        TEST_ASSERT_MESSAGE(init_addr + i == epts[i]->addr,
                            "'rpmsg_lite_create_ept' does not provide expected address");
    }
    /* Test bad args */
    /* Wrong rpmsg_lite_dev param */
    TEST_ASSERT_MESSAGE(RL_NULL == rpmsg_lite_create_ept(RL_NULL, init_addr, rpmsg_queue_rx_cb, NULL, &epts_ctxt[0]), "'rpmsg_lite_create_ept' with bad rpmsg_lite_dev param failed");
    return 0;
}

// utility: destroy number of epts
int32_t ts_destroy_epts(rpmsg_queue_handle queues[], struct rpmsg_lite_endpoint *volatile epts[], int32_t count)
{
    TEST_ASSERT_MESSAGE(queues != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");
    TEST_ASSERT_MESSAGE(count >= 3, "increase the TC_EPT_COUNT to be at least 3");
    
    //use different sequence of EP destroy to cover the case when EP is removed from the intermediate element of the EP linked list
    rpmsg_queue_destroy(my_rpmsg, queues[1]);
    rpmsg_lite_destroy_ept(my_rpmsg, epts[1]);
    rpmsg_queue_destroy(my_rpmsg, queues[0]);
    rpmsg_lite_destroy_ept(my_rpmsg, epts[0]);
    for (int32_t i = 2; i < count; i++)
    {
        rpmsg_queue_destroy(my_rpmsg, queues[i]);
        rpmsg_lite_destroy_ept(my_rpmsg, epts[i]);
    }
    /* Destroying an endpoint again when all endpoints are already destroyed */
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == rpmsg_lite_destroy_ept(my_rpmsg, epts[0]), "'rpmsg_lite_destroy_ept' being called when all endpoints are alrady destroyed failed");
    /* Test bad args */
    /* Wrong rpmsg_lite_dev param */
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == rpmsg_lite_destroy_ept(RL_NULL, epts[0]), "'rpmsg_lite_destroy_ept' with bad rpmsg_lite_dev param failed");
    /* Wrong rl_ept param */
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == rpmsg_lite_destroy_ept(my_rpmsg, RL_NULL), "'rpmsg_lite_destroy_ept' with bad rl_ept param failed");
    return 0;
}

/******************************************************************************
 * Test case 1
 * - verify simple transport between default epts of default channels
 * - verify simple nocopy transport between default epts of default channels
 * - verify simple transport between custom created epts of default channels
 * - verify simple nocopy transport between custom created epts of default channels
 *****************************************************************************/
void tc_1_defchnl_1(void)
{
    int32_t result = 0;

    // init rpmsg and environment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    result = ts_create_epts(&default_queue, &default_rpmsg_queue_storage[0], &default_queue_ctxt, &default_ept, &default_ept_ctxt, 1, TC_LOCAL_EPT_ADDR);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_create_epts' failed");
    if (result)
        goto end1;

    // wait some time to allow remote side to create epts and be ready for messages handling
    env_sleep_msec(200);

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        int32_t received = 0, send = i;
        uint32_t len = 0;
        uint32_t remote_addr = 0;
        rpmsg_lite_send(my_rpmsg, default_ept, TC_REMOTE_EPT_ADDR, (char *)&send, sizeof(send), RL_BLOCK);
        rpmsg_queue_recv(my_rpmsg, default_queue, &remote_addr, (char *)&received, sizeof(received), &len, RL_BLOCK);
        TEST_ASSERT_MESSAGE(received == i + 10, "'rpmsg_queue_recv' failed");
        TEST_ASSERT_MESSAGE(len == sizeof(send), "'rpmsg_queue_recv' failed");
    }

end1:
    result = rpmsg_queue_destroy(my_rpmsg, default_queue);
    TEST_ASSERT_MESSAGE(0 == result, "'rpmsg_queue_destroy' failed");
    default_queue = NULL;
    env_memset(&default_rpmsg_queue_storage, 0, RL_ENV_QUEUE_STATIC_STORAGE_SIZE);
    env_memset(&default_queue_ctxt, 0, sizeof(rpmsg_static_queue_ctxt));
    result = rpmsg_lite_destroy_ept(my_rpmsg, default_ept);
    TEST_ASSERT_MESSAGE(0 == result, "'rpmsg_lite_destroy_ept' failed");
    default_ept = NULL;
    env_memset(&default_ept_ctxt, 0, sizeof(struct rpmsg_lite_ept_static_context));
end0:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_deinit_rpmsg' failed");
}

void tc_1_defchnl_2(void)
{
    int32_t result = 0;

    // init rpmsg and environment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    result = ts_create_epts(&default_queue, &default_rpmsg_queue_storage[0], &default_queue_ctxt, &default_ept, &default_ept_ctxt, 1, TC_LOCAL_EPT_ADDR);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_create_epts' failed");
    if (result)
        goto end1;

    // wait some time to allow remote side to create epts and be ready for messages handling
    env_sleep_msec(200);

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        int32_t *received = NULL, send = i;
        uint32_t len = 0;
        uint32_t remote_addr = 0;
        rpmsg_lite_send(my_rpmsg, default_ept, TC_REMOTE_EPT_ADDR, (char *)&send, sizeof(send), RL_BLOCK);
        rpmsg_queue_recv_nocopy(my_rpmsg, default_queue, &remote_addr, (char **)&received, &len, RL_BLOCK);
        TEST_ASSERT_MESSAGE(*received == i + 10, "'rpmsg_queue_recv_nocopy' failed");
        TEST_ASSERT_MESSAGE(len == sizeof(send), "'rpmsg_queue_recv_nocopy' failed");
        rpmsg_queue_nocopy_free(my_rpmsg, received);
    }

end1:
    result = rpmsg_queue_destroy(my_rpmsg, default_queue);
    TEST_ASSERT_MESSAGE(0 == result, "'rpmsg_queue_destroy' failed");
    default_queue = NULL;
    env_memset(&default_rpmsg_queue_storage, 0, RL_ENV_QUEUE_STATIC_STORAGE_SIZE);
    env_memset(&default_queue_ctxt, 0, sizeof(rpmsg_static_queue_ctxt));
    result = rpmsg_lite_destroy_ept(my_rpmsg, default_ept);
    TEST_ASSERT_MESSAGE(0 == result, "'rpmsg_lite_destroy_ept' failed");
    default_ept = NULL;
    env_memset(&default_ept_ctxt, 0, sizeof(struct rpmsg_lite_ept_static_context));
end0:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_deinit_rpmsg' failed");
}

void tc_1_defchnl_3(void)
{
    struct rpmsg_lite_endpoint *volatile epts[TC_EPT_COUNT] = {0};
    struct rpmsg_lite_ept_static_context ept_ctxt[TC_EPT_COUNT];
    rpmsg_queue_handle queues[TC_EPT_COUNT] = {0};
    rpmsg_static_queue_ctxt queues_ctxt[TC_EPT_COUNT] = {0};
    uint8_t rpmsg_queue_storage[TC_EPT_COUNT][RL_ENV_QUEUE_STATIC_STORAGE_SIZE];
    int32_t result = 0;

    // init rpmsg and environment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    result = ts_create_epts(queues, &rpmsg_queue_storage[0][0], queues_ctxt, epts, ept_ctxt, TC_EPT_COUNT, TC_LOCAL_EPT_ADDR);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_create_epts' failed");
    if (result)
        goto end1;

    // wait some time to allow remote side to create epts and be ready for messages handling
    env_sleep_msec(200);

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        for (int32_t j = 0; j < TC_EPT_COUNT; j++)
        {
            int32_t received = 0, send = 0;
            uint32_t len = 0;
            send = 0;
            uint32_t remote_addr = 0;
            rpmsg_lite_send(my_rpmsg, epts[j], TC_REMOTE_EPT_ADDR + j, (char *)&send, sizeof(send), RL_BLOCK);
            rpmsg_queue_recv(my_rpmsg, queues[j], &remote_addr, (char *)&received, sizeof(received), &len, RL_BLOCK);
            TEST_ASSERT_MESSAGE(received == send + 10, "'rpmsg_queue_recv' failed");
            TEST_ASSERT_MESSAGE(remote_addr == (TC_REMOTE_EPT_ADDR + j), "'rpmsg_queue_recv' failed");
            TEST_ASSERT_MESSAGE(len == sizeof(received), "'rpmsg_queue_recv' failed");
        }
    }

end1:
    result = ts_destroy_epts(queues, epts, TC_EPT_COUNT);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_destroy_epts' failed");
end0:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_deinit_rpmsg' failed");
}

void tc_1_defchnl_4(void)
{
    struct rpmsg_lite_endpoint *epts[TC_EPT_COUNT] = {0};
    struct rpmsg_lite_ept_static_context ept_ctxt[TC_EPT_COUNT];
    rpmsg_queue_handle queues[TC_EPT_COUNT] = {0};
    rpmsg_static_queue_ctxt queues_ctxt[TC_EPT_COUNT] = {0};
    uint8_t rpmsg_queue_storage[TC_EPT_COUNT][RL_ENV_QUEUE_STATIC_STORAGE_SIZE];
    int32_t result = 0;

    // init rpmsg and environment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    result = ts_create_epts(queues, &rpmsg_queue_storage[0][0], queues_ctxt, epts, ept_ctxt, TC_EPT_COUNT, TC_LOCAL_EPT_ADDR);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_create_epts' failed");
    if (result)
        goto end1;

    // wait some time to allow remote side to create epts and be ready for messages handling
    env_sleep_msec(200);

    for (int32_t i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        for (int32_t j = 0; j < TC_EPT_COUNT; j++)
        {
            int32_t *received = NULL, send = 0;
            uint32_t len = 0;
            uint32_t remote_addr = 0;
            rpmsg_lite_send(my_rpmsg, epts[j], TC_REMOTE_EPT_ADDR + j, (char *)&send, sizeof(send), RL_BLOCK);
            rpmsg_queue_recv_nocopy(my_rpmsg, queues[j], &remote_addr, (char **)&received, &len, RL_BLOCK);
            TEST_ASSERT_MESSAGE(*received == send + 10, "'rpmsg_rtos_recv' failed");
            TEST_ASSERT_MESSAGE(remote_addr == (TC_REMOTE_EPT_ADDR + j), "'rpmsg_rtos_recv' failed");
            TEST_ASSERT_MESSAGE(len == sizeof(received), "'rpmsg_rtos_recv' failed");
            rpmsg_queue_nocopy_free(my_rpmsg, received);
        }
    }

end1:
    result = ts_destroy_epts(queues, epts, TC_EPT_COUNT);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_destroy_epts' failed");
end0:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_deinit_rpmsg' failed");
}

void tc_1_defchnl(void)
{
    for (int32_t i = 0; i < TC_INIT_COUNT; i++)
    {
        tc_1_defchnl_1();
    }
    for (int32_t i = 0; i < TC_INIT_COUNT; i++)
    {
        tc_1_defchnl_2();
    }
    for (int32_t i = 0; i < TC_INIT_COUNT; i++)
    {
        tc_1_defchnl_3();
    }
    for (int32_t i = 0; i < TC_INIT_COUNT; i++)
    {
        tc_1_defchnl_4();
    }
}

void run_tests(void *unused)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("02_epts_channels_rtos");
    __coveragescanner_install("02_epts_channels_rtos.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(tc_1_defchnl, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
