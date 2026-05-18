/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Test suite 06_rpmsg_adapter — secondary/remote core (HAL_RPMSG_SELECT_ROLE=1)
 *
 * This side mirrors the primary test cases:
 *   tc_1 — receives init signal, ACKs, then deinits
 *   tc_2 — receives TC2_PRIMARY_PAYLOAD, replies TC2_SECONDARY_PAYLOAD (x2)
 *   tc_3 — receives nocopy payload, echoes it back
 *   tc_4 — receives any payload, echoes it back (callback replacement on primary)
 *   tc_5 — no rpmsg activity (lowpower stubs are local-only on primary)
 *   tc_6 — regression: same as tc_1, confirms ready event is fired early enough
 *   tc_7 — echoes one message back on each of MAX_EP_COUNT endpoints
 *
 * Endpoint address layout (must match primary):
 *   TC_PRIMARY_EPT_ADDR  = 40   TC_SECONDARY_EPT_ADDR  = 30
 *   TC_NOCOPY_EPT_ADDR   = 42   TC_NOCOPY_SEC_EPT_ADDR = 32
 *   TC_MULTI_EPT_BASE    = 50   TC_MULTI_SEC_EPT_BASE  = 60
 *
 * Inter-test synchronisation uses a brief env_sleep_msec() delay between
 * test cases to ensure both cores complete teardown before re-init.
 */

#include "fsl_common.h"
#include "fsl_adapter_rpmsg.h"
#include "rpmsg_env.h"
#include "mcmgr.h"
#include "unity.h"
#include "app.h"
#include <string.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TC_PRIMARY_EPT_ADDR    (40U)
#define TC_SECONDARY_EPT_ADDR  (30U)
#define TC_NOCOPY_EPT_ADDR     (42U)
#define TC_NOCOPY_SEC_EPT_ADDR (32U)
#define TC_MULTI_EPT_BASE      (50U)
#define TC_MULTI_SEC_EPT_BASE  (60U)

#define TC2_PRIMARY_PAYLOAD    (0xDEADBEEFU)
#define TC2_SECONDARY_PAYLOAD  (0xCAFEBABEU)

/*******************************************************************************
 * Inter-test delay — mirrors primary (see primary/main.c for rationale).
 ******************************************************************************/
static void inter_test_sync(mcmgr_core_t peer)
{
    (void)peer;
    env_sleep_msec(150);
}

/*******************************************************************************
 * Globals
 ******************************************************************************/
static volatile uint32_t s_rxData     = 0U;
static volatile uint8_t  s_rxReceived = 0U;

/* Multi-endpoint: store received payloads for deferred send from main thread */
static volatile uint32_t s_multiRxPayload[MAX_EP_COUNT] = {0U};
static volatile uint8_t  s_multiRxReady[MAX_EP_COUNT]   = {0U};

RPMSG_HANDLE_DEFINE(s_rpmsgHandle);
RPMSG_HANDLE_DEFINE(s_nocopyHandle);
RPMSG_HANDLE_DEFINE(s_multiHandle[MAX_EP_COUNT]);

/*******************************************************************************
 * Helper
 ******************************************************************************/
static bool wait_flag(volatile uint8_t *flag, uint8_t expected, uint32_t retries)
{
    while (retries-- != 0U)
    {
        if (*flag == expected)
        {
            return true;
        }
    }
    return false;
}

/*******************************************************************************
 * Callbacks
 ******************************************************************************/

/*
 * General data channel callback:
 * - Stores received data and sets s_rxReceived flag.
 * - Does NOT call HAL_RpmsgSend here: on IMU/wireless platforms, calling
 *   copy-send (rpmsg_lite_send) from inside a receive ISR callback can
 *   deadlock because the IMU TX path spins waiting for a TX slot that
 *   cannot be serviced while the RX interrupt is still active.
 *   The reply is sent by the test body from main-thread context after
 *   wait_flag() returns.
 */
static hal_rpmsg_return_status_t data_rx_callback(void *param, uint8_t *data, uint32_t len)
{
    (void)param;

    if (len >= sizeof(uint32_t))
    {
        (void)memcpy((void *)&s_rxData, data, sizeof(uint32_t));
    }
    s_rxReceived = 1U;
    return kStatus_HAL_RL_RELEASE;
}

/* Zero-copy channel: echo the received payload back */
static volatile uint8_t s_nocopyRxOk = 0U;
static hal_rpmsg_return_status_t nocopy_rx_callback(void *param, uint8_t *data, uint32_t len)
{
    hal_rpmsg_handle_t handle = (hal_rpmsg_handle_t)param;
    uint32_t *buf;

    s_nocopyRxOk = 1U;

    if ((handle != NULL) && (len >= sizeof(uint32_t)))
    {
        buf = (uint32_t *)HAL_RpmsgAllocTxBuffer(handle, sizeof(uint32_t));
        if (buf != NULL)
        {
            (void)memcpy(buf, data, sizeof(uint32_t));
            (void)HAL_RpmsgNoCopySend(handle, (uint8_t *)buf, sizeof(uint32_t));
        }
    }
    return kStatus_HAL_RL_RELEASE;
}

/* Multi-endpoint echo callback.
 * Stores payload per-slot for deferred send from main thread (see tc_7).
 * The index is inferred from param (pointer to the matching handle slot). */
static volatile uint8_t s_multiRxCount = 0U;
static hal_rpmsg_return_status_t multi_rx_callback(void *param, uint8_t *data, uint32_t len)
{
    /* Identify which endpoint slot fired by matching param to handle array */
    uint8_t i;
    for (i = 0U; i < MAX_EP_COUNT; i++)
    {
        if (param == (void *)s_multiHandle[i])
        {
            if (len >= sizeof(uint32_t))
            {
                (void)memcpy((void *)&s_multiRxPayload[i], data, sizeof(uint32_t));
            }
            s_multiRxReady[i] = 1U;
            break;
        }
    }
    s_multiRxCount++;
    return kStatus_HAL_RL_RELEASE;
}

/*******************************************************************************
 * tc_6 — ep_ready_event regression (remote side, MUST run first)
 *
 * On the secondary, this is the first call to HAL_RpmsgMcmgrInit() which
 * calls MCMGR_Init() internally.
 ******************************************************************************/
void tc_6_adapter_ep_ready_event(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;

    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (sec tc6)");

    cfg.local_addr  = TC_SECONDARY_EPT_ADDR;
    cfg.remote_addr = TC_PRIMARY_EPT_ADDR;
    cfg.callback    = data_rx_callback;
    cfg.param       = NULL;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_rpmsgHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed (sec tc6)");

    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgDeinit failed (sec tc6)");

    inter_test_sync(kMCMGR_Core0);
}

/*******************************************************************************
 * tc_1 — init / deinit lifecycle (remote side)
 ******************************************************************************/
void tc_1_adapter_init_deinit(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;

    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (sec tc1)");

    cfg.local_addr  = TC_SECONDARY_EPT_ADDR;
    cfg.remote_addr = TC_PRIMARY_EPT_ADDR;
    cfg.callback    = data_rx_callback;
    cfg.param       = NULL;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_rpmsgHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed (sec tc1)");

    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgDeinit failed (sec tc1)");

    inter_test_sync(kMCMGR_Core0);
}

/*******************************************************************************
 * tc_2 — bidirectional send / receive (remote side)
 ******************************************************************************/
void tc_2_adapter_send_receive(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;
    uint32_t reply = TC2_SECONDARY_PAYLOAD;

    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (sec tc2)");

    cfg.local_addr  = TC_SECONDARY_EPT_ADDR;
    cfg.remote_addr = TC_PRIMARY_EPT_ADDR;
    cfg.callback    = data_rx_callback;
    cfg.param       = NULL;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_rpmsgHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed (sec tc2)");

    /*
     * Primary sends TC2_PRIMARY_PAYLOAD twice.
     * Wait for each receive, then send reply from main-thread context
     * (NOT from inside the ISR callback — copy-send from ISR deadlocks on
     * IMU/wireless platforms where the TX path cannot be serviced while
     * the RX interrupt is still active).
     */
    s_rxReceived = 0U;
    TEST_ASSERT_MESSAGE(true == wait_flag(&s_rxReceived, 1U, 10000000U),
                        "Timeout waiting for first send from primary (sec tc2)");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(TC2_PRIMARY_PAYLOAD, s_rxData,
                                    "Wrong payload received from primary (sec tc2, first)");
    ret = HAL_RpmsgSend((hal_rpmsg_handle_t)s_rpmsgHandle, (uint8_t *)&reply, sizeof(reply));
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgSend reply failed (sec tc2, first)");

    s_rxReceived = 0U;
    TEST_ASSERT_MESSAGE(true == wait_flag(&s_rxReceived, 1U, 10000000U),
                        "Timeout waiting for second send from primary (sec tc2)");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(TC2_PRIMARY_PAYLOAD, s_rxData,
                                    "Wrong payload received from primary (sec tc2, second)");
    ret = HAL_RpmsgSend((hal_rpmsg_handle_t)s_rpmsgHandle, (uint8_t *)&reply, sizeof(reply));
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgSend reply failed (sec tc2, second)");

    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgDeinit failed (sec tc2)");

    inter_test_sync(kMCMGR_Core0);
}

/*******************************************************************************
 * tc_3 — zero-copy send (remote side)
 ******************************************************************************/
void tc_3_adapter_nocopy_send(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;

    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (sec tc3)");

    cfg.local_addr  = TC_NOCOPY_SEC_EPT_ADDR;
    cfg.remote_addr = TC_NOCOPY_EPT_ADDR;
    cfg.callback    = nocopy_rx_callback;
    cfg.param       = (void *)s_nocopyHandle;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_nocopyHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed (sec tc3)");

    /* nocopy_rx_callback handles the echo; just wait for it */
    s_nocopyRxOk = 0U;
    TEST_ASSERT_MESSAGE(true == wait_flag(&s_nocopyRxOk, 1U, 10000000U),
                        "Timeout waiting for nocopy message from primary (sec tc3)");

    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_nocopyHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgDeinit failed (sec tc3)");

    inter_test_sync(kMCMGR_Core0);
}

/*******************************************************************************
 * tc_4 — callback replacement (remote side echoes one message)
 ******************************************************************************/
void tc_4_adapter_rx_callback(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;
    uint32_t reply = TC2_SECONDARY_PAYLOAD;

    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (sec tc4)");

    cfg.local_addr  = TC_SECONDARY_EPT_ADDR;
    cfg.remote_addr = TC_PRIMARY_EPT_ADDR;
    cfg.callback    = data_rx_callback;
    cfg.param       = NULL;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_rpmsgHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed (sec tc4)");

    /* Wait for primary's message then reply from main-thread context */
    s_rxReceived = 0U;
    TEST_ASSERT_MESSAGE(true == wait_flag(&s_rxReceived, 1U, 10000000U),
                        "Timeout waiting for message from primary (sec tc4)");
    ret = HAL_RpmsgSend((hal_rpmsg_handle_t)s_rpmsgHandle, (uint8_t *)&reply, sizeof(reply));
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgSend reply failed (sec tc4)");

    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgDeinit failed (sec tc4)");

    inter_test_sync(kMCMGR_Core0);
}

/*******************************************************************************
 * tc_5 — no remote action needed (lowpower stubs are primary-local)
 ******************************************************************************/
void tc_5_adapter_lowpower_stubs(void)
{
    /* Nothing to do on the secondary side for this test — just sync. */
    inter_test_sync(kMCMGR_Core0);
}

/*******************************************************************************
 * tc_7 — multiple endpoints (remote side)
 ******************************************************************************/
void tc_7_adapter_multi_ep(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;
    uint8_t i;

    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (sec tc7)");

    s_multiRxCount = 0U;
    for (i = 0U; i < MAX_EP_COUNT; i++)
    {
        s_multiRxReady[i]   = 0U;
        s_multiRxPayload[i] = 0U;
    }

    for (i = 0U; i < MAX_EP_COUNT; i++)
    {
        cfg.local_addr  = (uint8_t)(TC_MULTI_SEC_EPT_BASE + i);
        cfg.remote_addr = (uint8_t)(TC_MULTI_EPT_BASE + i);
        cfg.callback    = multi_rx_callback;
        cfg.param       = (void *)s_multiHandle[i];

        ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_multiHandle[i], &cfg);
        TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret,
                            "HAL_RpmsgInit failed (sec tc7, ep open)");
    }

    /* Wait for all messages to arrive, then send all echoes from main-thread
     * context (copy-send must not be called from inside a receive ISR on
     * IMU/wireless platforms). */
    TEST_ASSERT_MESSAGE(true == wait_flag(&s_multiRxCount, MAX_EP_COUNT, 50000000U),
                        "Timeout waiting to receive all multi-ep messages (sec tc7)");

    for (i = 0U; i < MAX_EP_COUNT; i++)
    {
        if (s_multiRxReady[i] != 0U)
        {
            uint32_t payload = (uint32_t)s_multiRxPayload[i];
            ret = HAL_RpmsgSend((hal_rpmsg_handle_t)s_multiHandle[i],
                                (uint8_t *)&payload, sizeof(payload));
            TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret,
                                "HAL_RpmsgSend echo failed (sec tc7)");
        }
    }

    for (i = MAX_EP_COUNT; i > 0U; i--)
    {
        ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_multiHandle[i - 1U]);
        TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret,
                            "HAL_RpmsgDeinit failed (sec tc7, ep close)");
    }

    inter_test_sync(kMCMGR_Core0);
}

/*******************************************************************************
 * Test runner
 ******************************************************************************/
void run_tests(void)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("06_rpmsg_adapter_sec_core");
    __coveragescanner_install("06_rpmsg_adapter_sec_core.csexe");
#endif /* __COVERAGESCANNER__ */
    /* Must match primary run order exactly: tc_6 first. */
    RUN_EXAMPLE(tc_6_adapter_ep_ready_event, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_EXAMPLE(tc_1_adapter_init_deinit,    MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    RUN_EXAMPLE(tc_2_adapter_send_receive,   MAKE_UNITY_NUM(k_unity_rpmsg, 2));
    RUN_EXAMPLE(tc_3_adapter_nocopy_send,    MAKE_UNITY_NUM(k_unity_rpmsg, 3));
    RUN_EXAMPLE(tc_4_adapter_rx_callback,    MAKE_UNITY_NUM(k_unity_rpmsg, 4));
    RUN_EXAMPLE(tc_5_adapter_lowpower_stubs, MAKE_UNITY_NUM(k_unity_rpmsg, 5));
    RUN_EXAMPLE(tc_7_adapter_multi_ep,       MAKE_UNITY_NUM(k_unity_rpmsg, 6));
}
