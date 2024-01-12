03_send_receive_rtos test suite

 - FreeRTOS/ThreadX/XOS-based project, covering both the static allocation (#define RL_USE_STATIC_API (1) in rpmsg_config.h) and the dynamic allocation (#define RL_USE_STATIC_API (0) in rpmsg_config.h)

 - test all send functions with valid/invalid parameters
 - test all receive functions with valid/invalid parameters