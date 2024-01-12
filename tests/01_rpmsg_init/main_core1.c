/*
 * Copyright 2016-2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rpmsg_lite.h"
#include "unity.h"
#include "rpmsg_ns.h"
#include "assert.h"
#include "app.h"

#include "fsl_common.h"
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
#include "fsl_memory.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
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
uint32_t test_counter = 0;

void tc_1_rpmsg_init()
{
    struct rpmsg_lite_instance *my_rpmsg;
    struct rpmsg_lite_instance rpmsg_ctxt;
    int32_t result = 0;

    for (test_counter = 0; test_counter < 2; test_counter++)
    {
#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
        my_rpmsg = rpmsg_lite_remote_init(rpmsg_lite_base+(2*test_counter), RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#else
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
        my_rpmsg = rpmsg_lite_remote_init((void *)MEMORY_ConvertMemoryMapAddress((uint32_t)RPMSG_LITE_SHMEM_BASE, kMEMORY_DMA2Local), RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#else
        my_rpmsg = rpmsg_lite_remote_init((void *)(RPMSG_LITE_SHMEM_BASE+(2*test_counter)), RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
#endif
#endif /* SH_MEM_NOT_TAKEN_FROM_LINKER */
        TEST_ASSERT_MESSAGE(NULL != my_rpmsg, "init function failed");

        /* incomming interrupt changes state to state_created_channel */
        rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);
        TEST_ASSERT_MESSAGE(1 == rpmsg_lite_is_link_up(my_rpmsg), "rpmsg_lite_is_link_up function failed");
        result = rpmsg_lite_deinit(my_rpmsg);
        TEST_ASSERT_MESSAGE(RL_SUCCESS == result, "deinit function failed");
        TEST_ASSERT_MESSAGE(RL_SUCCESS != my_rpmsg, "deinit function failed");
        
#ifdef __COVERAGESCANNER__
        /* Calling rpmsg_lite_deinit() twice should fail, tested when RL_ASSERT is off in Coco tests */
        result = rpmsg_lite_deinit(my_rpmsg);
        TEST_ASSERT_MESSAGE(RL_ERR_PARAM == result, "repeated deinit function call failed");
#endif /*__COVERAGESCANNER__*/

        TEST_ASSERT_MESSAGE(RL_FALSE == rpmsg_lite_is_link_up(my_rpmsg), "link should be down");

        /* Wait for remote side to re-initialize the rpmsg.*/
        //env_sleep_msec(1000);
    }
    
#ifdef __COVERAGESCANNER__
    /* When RL_ASSERT is off in Coco tests */
    TEST_ASSERT_MESSAGE(-1 == env_deinit(), "env_deinit being called repeatedly failed");
#endif /*__COVERAGESCANNER__*/

    /* Test bad args */
    TEST_ASSERT_MESSAGE(0 == rpmsg_lite_is_link_up(RL_NULL), "rpmsg_lite_is_link_up function with bad rpmsg_lite_dev param failed");
    TEST_ASSERT_MESSAGE(0 == rpmsg_lite_wait_for_link_up(RL_NULL, RL_BLOCK), "rpmsg_lite_wait_for_link_up function with bad rpmsg_lite_dev param failed");
#ifndef SH_MEM_NOT_TAKEN_FROM_LINKER
    /* Wrong link_id param */
    my_rpmsg = rpmsg_lite_remote_init(rpmsg_lite_base, RPMSG_LITE_LINK_ID+1, RL_NO_FLAGS, &rpmsg_ctxt);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "init function with bad link_id param failed");
    /* Wrong shmem_addr param */
    my_rpmsg = rpmsg_lite_remote_init(RL_NULL, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "init function with bad shmem_addr param failed");
    /* Wrong static_context param */
    my_rpmsg = rpmsg_lite_remote_init(rpmsg_lite_base, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, RL_NULL);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "init function with bad static_context param failed");
#else
#if (defined(FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET) && FSL_FEATURE_MEMORY_HAS_ADDRESS_OFFSET)
    /* Wrong link_id param */
    my_rpmsg = rpmsg_lite_remote_init((void *)MEMORY_ConvertMemoryMapAddress((uint32_t)RPMSG_LITE_SHMEM_BASE, kMEMORY_DMA2Local), RPMSG_LITE_LINK_ID+1, RL_NO_FLAGS, &rpmsg_ctxt);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "init function with bad link_id param failed");
    /* Wrong shmem_addr param */
    my_rpmsg = rpmsg_lite_remote_init(RL_NULL, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "init function with bad shmem_addr param failed");
    /* Wrong static_context param */
    my_rpmsg = rpmsg_lite_remote_init((void *)MEMORY_ConvertMemoryMapAddress((uint32_t)RPMSG_LITE_SHMEM_BASE, kMEMORY_DMA2Local), RPMSG_LITE_LINK_ID, RL_NO_FLAGS, RL_NULL);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "init function with bad static_context param failed");
#else
    /* Wrong link_id param */
    my_rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID+1, RL_NO_FLAGS, &rpmsg_ctxt);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "init function with bad link_id param failed");
    /* Wrong shmem_addr param */
    my_rpmsg = rpmsg_lite_remote_init(RL_NULL, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, &rpmsg_ctxt);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "init function with bad shmem_addr param failed");
    /* Wrong static_context param */
    my_rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS, RL_NULL);
    TEST_ASSERT_MESSAGE(RL_NULL == my_rpmsg, "init function with bad static_context param failed");
#endif
#endif /* SH_MEM_NOT_TAKEN_FROM_LINKER */

    /* Wrong rpmsg_lite_instance param */
    TEST_ASSERT_MESSAGE(RL_ERR_PARAM == rpmsg_lite_deinit(RL_NULL), "deinit function with bad rpmsg_lite_instance param failed");
}

void tc_2_env_testing()
{
    uint32_t *temp1 = env_allocate_memory(sizeof(uint32_t));
    TEST_ASSERT_MESSAGE(RL_NULL != temp1, "env_allocate_memory function failed");
    env_free_memory(temp1);
    env_free_memory(RL_NULL);
    uint32_t temp2 = 0x11, temp3 = 0x22;
    env_memset(&temp2, 0, sizeof(uint32_t));
    TEST_ASSERT_MESSAGE(0 == temp2, "env_memset function failed");
    env_memcpy(&temp3, &temp2, sizeof(uint32_t));
    TEST_ASSERT_MESSAGE(0 == temp3, "env_memcpy function failed");
    char str1[5], str2[5];
    env_strncpy(str1, "abc", 4);
    env_strncpy(str2, "ABC", 4);
    TEST_ASSERT_MESSAGE(0 < env_strcmp(str1, str2), "env_strcmp function failed");
    TEST_ASSERT_MESSAGE(0 < env_strncmp(str1, str2, 4), "env_strncmp function failed");
    env_mb();
    env_rmb();
    env_wmb();
    TEST_ASSERT_MESSAGE(0 != env_map_vatopa((void*)0x00000010), "env_map_vatopa function failed");
    TEST_ASSERT_MESSAGE(RL_NULL != env_map_patova(0x00000010), "env_map_patova function failed");
    void *mutex = NULL;
    //TEST_ASSERT_MESSAGE(-1 == env_create_mutex(&mutex, 100), "env_create_mutex function with bad count param failed"); does not make sense in baremetal
#if defined(RL_USE_STATIC_API) && (RL_USE_STATIC_API == 1)
    TEST_ASSERT_MESSAGE(0 == env_create_mutex(&mutex, 1, NULL), "env_create_mutex function failed");
#else
    TEST_ASSERT_MESSAGE(0 == env_create_mutex(&mutex, 1), "env_create_mutex function failed");
#endif
    env_lock_mutex(mutex);
    env_unlock_mutex(mutex);
    env_delete_mutex(mutex);    
    env_sleep_msec(1);
    env_map_memory(temp2, temp3, sizeof(uint32_t), 0);
    env_disable_cache();
    //TEST_ASSERT_MESSAGE(0 < env_get_timestamp(), "env_get_timestamp function failed");
    platform_time_delay(1);
#ifdef __COVERAGESCANNER__
    //Test incorrect access to the isr_table when RL_ASSERT is off in Coco tests 
    env_register_isr(0xffffffff, RL_NULL);
    env_unregister_isr(0xffffffff);
    env_isr(0xffffffff);
#endif /*__COVERAGESCANNER__*/
}

void run_tests()
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("01_rpmsg_init_sec_core");
    __coveragescanner_install("01_rpmsg_init_sec_core.csexe");
#endif /*__COVERAGESCANNER__*/
    RUN_EXAMPLE(tc_1_rpmsg_init, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_EXAMPLE(tc_2_env_testing, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
}
