Test suite 06_rpmsg_adapter
===========================

Tests the fsl_adapter_rpmsg HAL component (fsl_adapter_rpmsg.c/.h).

Test cases (primary/master side):
  tc_1_adapter_init_deinit       - HAL_RpmsgMcmgrInit / HAL_RpmsgInit / HAL_RpmsgDeinit lifecycle,
                                   idempotent re-init guard (s_rpmsg_init_golbal).
  tc_2_adapter_send_receive      - HAL_RpmsgSend / HAL_RpmsgSendTimeout bidirectional data transfer,
                                   rx callback invocation.
  tc_3_adapter_nocopy_send       - HAL_RpmsgAllocTxBuffer / HAL_RpmsgAllocTxBufferTimeout /
                                   HAL_RpmsgNoCopySend zero-copy path.
  tc_4_adapter_rx_callback       - HAL_RpmsgInstallRxCallback replaces callback post-init.
  tc_5_adapter_lowpower_stubs    - HAL_RpmsgEnterLowpower / HAL_RpmsgExitLowpower return
                                   kStatus_HAL_RpmsgError (stub coverage).
  tc_6_adapter_ep_ready_event    - Regression: MCMGR_RegisterEvent called before MCMGR_Init must
                                   not fail; remote ready event received within timeout.
  tc_7_adapter_multi_ep          - Up to MAX_EP_COUNT endpoints created, used, and destroyed.

Both primary and secondary use static rpmsg-lite allocation (RL_USE_STATIC_API=1).
Primary is compiled with HAL_RPMSG_SELECT_ROLE=0 (master).
Secondary is compiled with HAL_RPMSG_SELECT_ROLE=1 (remote).
