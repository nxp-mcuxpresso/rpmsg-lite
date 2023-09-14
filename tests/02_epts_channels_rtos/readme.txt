02_epts_channels_rtos test suite

 - FreeRTOS/ThreadX/XOS-based project, covering both the static allocation (#define RL_USE_STATIC_API (1) in rpmsg_config.h) and the dynamic allocation (#define RL_USE_STATIC_API (0) in rpmsg_config.h)

 - send user data through default channel
 - create several custom epts and send data through them
