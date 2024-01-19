/*
 * Copyright 2016-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rpmsg_lite.h"
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
#define TC_INIT_COUNT 24
#define TC_TRANSFER_COUNT 10
#define TC_EPT_COUNT 3
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
volatile int32_t message_received = 0;
int32_t trans_data = 0;
uint32_t trans_src = 0;
struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
struct rpmsg_lite_instance rpmsg_ctxt;

// custom ept callback
int32_t ept_cb(void *payload, uint32_t payload_len, uint32_t src, void *priv)
{
    TEST_ASSERT_MESSAGE(0 == message_received, "interrupt miss");
    trans_data = *((int32_t *)payload);
    trans_src = src;
    message_received = 1;

    return RL_RELEASE; /* let RPMsg lite know it can free the received data */
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
    return 0;
}

// utility: deinitialize rpmsg and environment
int32_t ts_deinit_rpmsg(void)
{
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
    /* Test bad args */
    /* Wrong rpmsg_lite_dev param */
    TEST_ASSERT_MESSAGE(RL_NULL == rpmsg_lite_create_ept(RL_NULL, init_addr, ept_cb, NULL, &ctxts[0]), "'rpmsg_lite_create_ept' with bad rpmsg_lite_dev param failed");
    /* Wrong ept_context param */
    TEST_ASSERT_MESSAGE(RL_NULL == rpmsg_lite_create_ept(my_rpmsg, init_addr + count, ept_cb, NULL, RL_NULL), "'rpmsg_lite_create_ept' with bad ept_context param failed");
    return 0;
}

// utility: destroy number of epts
int32_t ts_destroy_epts(struct rpmsg_lite_endpoint *volatile epts[], int32_t count)
{
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");
    TEST_ASSERT_MESSAGE(count >= 3, "increase the TC_EPT_COUNT to be at least 3");
    
    //use different sequence of EP destroy to cover the case when EP is removed from the intermediate element of the EP linked list
    rpmsg_lite_destroy_ept(my_rpmsg, epts[1]);
    rpmsg_lite_destroy_ept(my_rpmsg, epts[0]);
    for (int32_t i = 2; i < count; i++)
    {
        rpmsg_lite_destroy_ept(my_rpmsg, epts[i]);
    }
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
 * - verify create/delete/recreation of epts with same address
 * - verify simple transport between custom created epts of default channels
 *****************************************************************************/
void tc_1_defchnl_transport_receive(struct rpmsg_lite_endpoint *src, int32_t dst, int32_t reply_value, int32_t expected_value)
{
    void *mutex = NULL;
    int32_t result = 0;
    result = env_create_mutex(&mutex, 1, NULL);
    TEST_ASSERT_MESSAGE(0 == result, "unexpected value");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    TEST_ASSERT_MESSAGE(trans_data == expected_value, "unexpected value");
    TEST_ASSERT_MESSAGE(src != NULL, "src is NULL");
    result = rpmsg_lite_send(my_rpmsg, src, dst, (char *)&reply_value, sizeof(trans_data), RL_BLOCK);
    TEST_ASSERT_MESSAGE(RL_SUCCESS == result, "send failed");
    env_unlock_mutex(mutex);
    env_delete_mutex(mutex);
}

void tc_1_defchnl_transport(void)
{
    struct rpmsg_lite_endpoint *volatile epts[TC_EPT_COUNT] = {0};
    struct rpmsg_lite_ept_static_context epts_ctxts[TC_EPT_COUNT];
    int32_t result = 0;

    // init rpmsg and environment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    // create custom epts of default channel, check nonrecoverable error
    result = ts_create_epts(epts, TC_EPT_COUNT, TC_LOCAL_EPT_ADDR, epts_ctxts);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_create_epts' failed");
    if (result)
        goto end1;

    // transfer data through custom epts
    for (int32_t j = 0; j < TC_TRANSFER_COUNT; j++)
    {
        for (int32_t i = 0; i < TC_EPT_COUNT; i++)
        {
            // receive message and reply
            tc_1_defchnl_transport_receive(epts[i],                // src ept
                                           TC_REMOTE_EPT_ADDR + i, // dst addr
                                           i + 10,                 // value to reply
                                           i                       // expected value to receive
                                           );
        }
    }

end1:
    result = ts_destroy_epts(epts, TC_EPT_COUNT);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_destroy_epts' failed");
end0:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_deinit_rpmsg' failed");
}

void tc_1_defchnl(void)
{
    for (int32_t i = 0; i < TC_INIT_COUNT; i++)
    {
        tc_1_defchnl_transport();
    }
}

void run_tests(void *unused)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("02_epts_channels_sec_core");
    __coveragescanner_install("02_epts_channels_sec_core.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(tc_1_defchnl, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
