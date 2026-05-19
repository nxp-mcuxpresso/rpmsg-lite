/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Test suite 06_rpmsg_adapter — primary/master core (HAL_RPMSG_SELECT_ROLE=0)
 *
 * Tests the fsl_adapter_rpmsg HAL layer on top of rpmsg-lite + MCMGR.
 * Each test case synchronises with the secondary core via rpmsg endpoint
 * addresses:
 *   TC_PRIMARY_EPT_ADDR  / TC_SECONDARY_EPT_ADDR  — general data channel
 *   TC_NOCOPY_EPT_ADDR   / TC_NOCOPY_SEC_EPT_ADDR — zero-copy channel
 *   TC_MULTI_EPT_BASE    / TC_MULTI_SEC_EPT_BASE   — multi-endpoint sweep
 *
 * Inter-test synchronisation uses a brief env_sleep_msec() delay between
 * test cases to ensure both cores complete teardown before re-init.
 */

#include "fsl_common.h"
#include "fsl_adapter_rpmsg.h"
#include "mcmgr.h"
#include "rpmsg_env.h"
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

/* Test payload */
#define TC2_PRIMARY_PAYLOAD    (0xDEADBEEFU)
#define TC2_SECONDARY_PAYLOAD  (0xCAFEBABEU)

/* Spin-wait retry count for wait_flag() calls.
 * Default suits Cortex-M33/M7 at ≥100 MHz (≈200 ms at 3 cycles/iter).
 * Override per-board via reconfig.cmake for slower cores (e.g. wireless
 * NBU at ~64 MHz: -DTC_WAIT_RETRY_COUNT=50000000U). */
#ifndef TC_WAIT_RETRY_COUNT
#define TC_WAIT_RETRY_COUNT (10000000U)
#endif

/*******************************************************************************
 * Inter-test delay
 *
 * A brief sleep between test cases ensures that both cores have fully
 * completed their HAL_RpmsgDeinit before the next HAL_RpmsgMcmgrInit cycle
 * begins.  An MCMGR event-based barrier was attempted previously but suffers
 * from irreconcilable registration-vs-fire races (secondary can fire the
 * barrier token before primary has registered the handler, particularly in
 * tc_6 where primary blocks on READY while secondary runs freely).
 ******************************************************************************/
static void inter_test_sync(mcmgr_core_t peer)
{
    (void)peer;
    env_sleep_msec(150);
}

/*******************************************************************************
 * Globals
 ******************************************************************************/
static volatile uint32_t s_rxData       = 0U;
static volatile uint8_t  s_rxReceived   = 0U;
static volatile uint32_t s_nocopyRxData = 0U;
static volatile uint8_t  s_nocopyRxOk   = 0U;

/* Handles for the general data channel */
RPMSG_HANDLE_DEFINE(s_rpmsgHandle);

/* Handles for zero-copy channel */
RPMSG_HANDLE_DEFINE(s_nocopyHandle);

/* Handles for multi-endpoint test (MAX_EP_COUNT = 5) */
RPMSG_HANDLE_DEFINE(s_multiHandle[MAX_EP_COUNT]);

/*******************************************************************************
 * Callbacks
 ******************************************************************************/
static hal_rpmsg_return_status_t rx_callback(void *param, uint8_t *data, uint32_t len)
{
    (void)param;
    if (len >= sizeof(uint32_t))
    {
        (void)memcpy((void *)&s_rxData, data, sizeof(uint32_t));
    }
    s_rxReceived = 1U;
    return kStatus_HAL_RL_RELEASE;
}

static hal_rpmsg_return_status_t nocopy_rx_callback(void *param, uint8_t *data, uint32_t len)
{
    (void)param;
    if (len >= sizeof(uint32_t))
    {
        (void)memcpy((void *)&s_nocopyRxData, data, sizeof(uint32_t));
    }
    s_nocopyRxOk = 1U;
    return kStatus_HAL_RL_RELEASE;
}

/* Replacement callback installed by tc_4 */
static volatile uint8_t s_replacedCbFired = 0U;
static hal_rpmsg_return_status_t replaced_rx_callback(void *param, uint8_t *data, uint32_t len)
{
    (void)param;
    (void)data;
    (void)len;
    s_replacedCbFired = 1U;
    return kStatus_HAL_RL_RELEASE;
}

/* Per-endpoint callback for multi-ep test */
static volatile uint8_t s_multiRxCount = 0U;
static hal_rpmsg_return_status_t multi_rx_callback(void *param, uint8_t *data, uint32_t len)
{
    (void)param;
    (void)data;
    (void)len;
    s_multiRxCount++;
    return kStatus_HAL_RL_RELEASE;
}

/*******************************************************************************
 * Helper: busy-wait on a volatile flag with a spin-limit
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
 * tc_6 — MCMGR_RegisterEvent-before-Init regression
 *
 * MUST run first so that it exercises the very first call to
 * HAL_RpmsgMcmgrInit() — the only call that enters the
 * MCMGR_RegisterEvent branch (s_rpmsg_init_golbal == 0).
 * Once s_rpmsg_init_golbal is set to 1 by this call, the branch is never
 * entered again regardless of how many deinit/reinit cycles follow.
 *
 * If a MCMGR_CHECK_INIT guard is (re)introduced in MCMGR_RegisterEvent the
 * registration silently fails, the READY handler is never installed, Core1
 * fires APP_RPMSG_READY_EVENT_DATA which the ISR discards (NULL callback),
 * and HAL_RpmsgMcmgrInit() times out.  Running tc_6 first makes this
 * immediately visible.
 ******************************************************************************/
void tc_6_adapter_ep_ready_event(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;

    /*
     * HAL_RpmsgMcmgrInit() internally calls:
     *   MCMGR_RegisterEvent(...)   <- must not fail (no MCMGR_CHECK_INIT guard)
     *   MCMGR_Init()
     *   MCMGR_StartCore(kMCMGR_Core1, ...)
     * then spins on RPMsgRemoteReadyEventData == APP_RPMSG_READY_EVENT_DATA.
     * If the event is missed the call returns kStatus_HAL_RpmsgTimeout.
     */
    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret,
                        "tc6: HAL_RpmsgMcmgrInit timed out \xe2\x80\x94 ready event missed (regression)");

    cfg.local_addr  = TC_PRIMARY_EPT_ADDR;
    cfg.remote_addr = TC_SECONDARY_EPT_ADDR;
    cfg.callback    = rx_callback;
    cfg.param       = NULL;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_rpmsgHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret,
                        "tc6: HAL_RpmsgInit failed after successful McmgrInit");

    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "tc6: HAL_RpmsgDeinit failed");

    inter_test_sync(kMCMGR_Core1);
}

/*******************************************************************************
 * tc_1 — init / deinit lifecycle
 ******************************************************************************/
void tc_1_adapter_init_deinit(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;

    /* First call: must succeed */
    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit first call failed");

    /* Second call: idempotent guard (s_rpmsg_init_golbal already 1U) — still success */
    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit second call failed");

    /* Open an endpoint */
    cfg.local_addr  = TC_PRIMARY_EPT_ADDR;
    cfg.remote_addr = TC_SECONDARY_EPT_ADDR;
    cfg.callback    = rx_callback;
    cfg.param       = NULL;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_rpmsgHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed");

    /* Deinit endpoint: count goes 0->-1, triggers rpmsg_lite_deinit internally */
    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgDeinit failed");

    inter_test_sync(kMCMGR_Core1);
}

/*******************************************************************************
 * tc_2 — bidirectional send / receive
 ******************************************************************************/
void tc_2_adapter_send_receive(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;
    uint32_t payload       = TC2_PRIMARY_PAYLOAD;

    /* Re-init for this test */
    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (tc2)");

    cfg.local_addr  = TC_PRIMARY_EPT_ADDR;
    cfg.remote_addr = TC_SECONDARY_EPT_ADDR;
    cfg.callback    = rx_callback;
    cfg.param       = NULL;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_rpmsgHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed (tc2)");

    /* --- Send primary->secondary --- */
    s_rxReceived = 0U;
    ret = HAL_RpmsgSend((hal_rpmsg_handle_t)s_rpmsgHandle, (uint8_t *)&payload, sizeof(payload));
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgSend failed");

    /* --- Receive secondary->primary reply --- */
    TEST_ASSERT_MESSAGE(true == wait_flag(&s_rxReceived, 1U, TC_WAIT_RETRY_COUNT),
                        "Timeout waiting for rx_callback from secondary");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(TC2_SECONDARY_PAYLOAD, s_rxData,
                                    "Received wrong payload from secondary");

    /* --- HAL_RpmsgSendTimeout variant --- */
    s_rxReceived = 0U;
    ret = HAL_RpmsgSendTimeout((hal_rpmsg_handle_t)s_rpmsgHandle, (uint8_t *)&payload,
                               sizeof(payload), RPMSG_WAITFOREVER);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgSendTimeout failed");
    TEST_ASSERT_MESSAGE(true == wait_flag(&s_rxReceived, 1U, TC_WAIT_RETRY_COUNT),
                        "Timeout waiting for second rx_callback from secondary");

    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgDeinit failed (tc2)");

    inter_test_sync(kMCMGR_Core1);
}

/*******************************************************************************
 * tc_3 — zero-copy send path
 ******************************************************************************/
void tc_3_adapter_nocopy_send(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;
    uint32_t *buf;
    uint32_t payload = 0x12345678U;

    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (tc3)");

    cfg.local_addr  = TC_NOCOPY_EPT_ADDR;
    cfg.remote_addr = TC_NOCOPY_SEC_EPT_ADDR;
    cfg.callback    = nocopy_rx_callback;
    cfg.param       = NULL;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_nocopyHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed (tc3)");

    /* Allocate tx buffer (blocking) */
    buf = (uint32_t *)HAL_RpmsgAllocTxBuffer((hal_rpmsg_handle_t)s_nocopyHandle,
                                             sizeof(uint32_t));
    TEST_ASSERT_MESSAGE(NULL != buf, "HAL_RpmsgAllocTxBuffer returned NULL");

    *buf = payload;

    s_nocopyRxOk = 0U;
    ret = HAL_RpmsgNoCopySend((hal_rpmsg_handle_t)s_nocopyHandle, (uint8_t *)buf,
                              sizeof(uint32_t));
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgNoCopySend failed");

    /* Secondary echoes back — wait for our nocopy rx callback */
    TEST_ASSERT_MESSAGE(true == wait_flag(&s_nocopyRxOk, 1U, TC_WAIT_RETRY_COUNT),
                        "Timeout waiting for nocopy rx echo from secondary");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(payload, s_nocopyRxData,
                                    "NoCopy echo payload mismatch");

    /* Also test HAL_RpmsgAllocTxBufferTimeout non-blocking (0 timeout) */
    (void)HAL_RpmsgAllocTxBufferTimeout((hal_rpmsg_handle_t)s_nocopyHandle, sizeof(uint32_t), 0U);

    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_nocopyHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgDeinit failed (tc3)");

    inter_test_sync(kMCMGR_Core1);
}

/*******************************************************************************
 * tc_4 — HAL_RpmsgInstallRxCallback
 ******************************************************************************/
void tc_4_adapter_rx_callback(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;
    uint32_t payload = 0xABCDEF01U;

    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (tc4)");

    cfg.local_addr  = TC_PRIMARY_EPT_ADDR;
    cfg.remote_addr = TC_SECONDARY_EPT_ADDR;
    cfg.callback    = rx_callback;   /* initial callback */
    cfg.param       = NULL;

    ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_rpmsgHandle, &cfg);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed (tc4)");

    /* Replace the callback */
    s_replacedCbFired = 0U;
    ret = HAL_RpmsgInstallRxCallback((hal_rpmsg_handle_t)s_rpmsgHandle,
                                     replaced_rx_callback, NULL);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInstallRxCallback failed");

    /* Send a message to secondary which will reply; the replaced callback should fire */
    ret = HAL_RpmsgSend((hal_rpmsg_handle_t)s_rpmsgHandle, (uint8_t *)&payload, sizeof(payload));
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgSend failed (tc4)");

    TEST_ASSERT_MESSAGE(true == wait_flag(&s_replacedCbFired, 1U, TC_WAIT_RETRY_COUNT),
                        "Replaced callback was not invoked");

    ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgDeinit failed (tc4)");

    inter_test_sync(kMCMGR_Core1);
}

/*******************************************************************************
 * tc_5 — low-power stubs (always return error — coverage only)
 ******************************************************************************/
void tc_5_adapter_lowpower_stubs(void)
{
    hal_rpmsg_status_t ret;

    ret = HAL_RpmsgEnterLowpower((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgError == ret,
                        "HAL_RpmsgEnterLowpower should return kStatus_HAL_RpmsgError");

    ret = HAL_RpmsgExitLowpower((hal_rpmsg_handle_t)s_rpmsgHandle);
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgError == ret,
                        "HAL_RpmsgExitLowpower should return kStatus_HAL_RpmsgError");

    inter_test_sync(kMCMGR_Core1);
}

/*******************************************************************************
 * tc_7 — multiple endpoints (up to MAX_EP_COUNT)
 ******************************************************************************/
void tc_7_adapter_multi_ep(void)
{
    hal_rpmsg_config_t cfg = {0};
    hal_rpmsg_status_t ret;
    uint32_t payload = 0x11223344U;
    uint8_t i;

    ret = HAL_RpmsgMcmgrInit();
    TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgMcmgrInit failed (tc7)");

    /* Open MAX_EP_COUNT endpoints on distinct address pairs */
    s_multiRxCount = 0U;
    for (i = 0U; i < MAX_EP_COUNT; i++)
    {
        cfg.local_addr  = (uint8_t)(TC_MULTI_EPT_BASE + i);
        cfg.remote_addr = (uint8_t)(TC_MULTI_SEC_EPT_BASE + i);
        cfg.callback    = multi_rx_callback;
        cfg.param       = NULL;

        ret = HAL_RpmsgInit((hal_rpmsg_handle_t)s_multiHandle[i], &cfg);
        TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgInit failed (tc7, ep open)");
    }

    /* Send one message per endpoint; secondary echoes each one back */
    for (i = 0U; i < MAX_EP_COUNT; i++)
    {
        payload = (uint32_t)(0x11000000U | i);
        ret = HAL_RpmsgSend((hal_rpmsg_handle_t)s_multiHandle[i], (uint8_t *)&payload,
                            sizeof(payload));
        TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret, "HAL_RpmsgSend failed (tc7)");
    }

    /* Wait for all echoes */
    TEST_ASSERT_MESSAGE(true == wait_flag(&s_multiRxCount, MAX_EP_COUNT, 50000000U),
                        "Timeout waiting for all multi-ep echoes from secondary");

    /* Deinit in reverse order */
    for (i = MAX_EP_COUNT; i > 0U; i--)
    {
        ret = HAL_RpmsgDeinit((hal_rpmsg_handle_t)s_multiHandle[i - 1U]);
        TEST_ASSERT_MESSAGE(kStatus_HAL_RpmsgSuccess == ret,
                            "HAL_RpmsgDeinit failed (tc7, ep close)");
    }

    inter_test_sync(kMCMGR_Core1);
}

/*******************************************************************************
 * Test runner
 ******************************************************************************/
void run_tests(void)
{
#ifdef __COVERAGESCANNER__
    __coveragescanner_testname("06_rpmsg_adapter");
    __coveragescanner_install("06_rpmsg_adapter.csexe");
#endif /* __COVERAGESCANNER__ */
    /* tc_6 MUST run first: it tests the very first call to HAL_RpmsgMcmgrInit()
     * where MCMGR_RegisterEvent() is invoked before MCMGR_Init().  If a
     * MCMGR_CHECK_INIT guard is (re)introduced in MCMGR_RegisterEvent this
     * call silently fails and the READY handler is never registered, causing
     * HAL_RpmsgMcmgrInit() to time out.  Once s_rpmsg_init_golbal=1 is set by
     * any prior test the registration block is never entered again, so moving
     * tc_6 anywhere else in the sequence makes it unable to catch that bug. */
    RUN_EXAMPLE(tc_6_adapter_ep_ready_event, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_EXAMPLE(tc_1_adapter_init_deinit,    MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    RUN_EXAMPLE(tc_2_adapter_send_receive,   MAKE_UNITY_NUM(k_unity_rpmsg, 2));
    RUN_EXAMPLE(tc_3_adapter_nocopy_send,    MAKE_UNITY_NUM(k_unity_rpmsg, 3));
    RUN_EXAMPLE(tc_4_adapter_rx_callback,    MAKE_UNITY_NUM(k_unity_rpmsg, 4));
    RUN_EXAMPLE(tc_5_adapter_lowpower_stubs, MAKE_UNITY_NUM(k_unity_rpmsg, 5));
    RUN_EXAMPLE(tc_7_adapter_multi_ep,       MAKE_UNITY_NUM(k_unity_rpmsg, 6));
}
