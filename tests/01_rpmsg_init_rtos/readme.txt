01_rpmsg_init_rtos test suite

 - FreeRTOS/ThreadX/XOS-based project, covering both the static allocation (#define RL_USE_STATIC_API (1) in rpmsg_config.h) and the dynamic allocation (#define RL_USE_STATIC_API (0) in rpmsg_config.h)

 - check multiple rpmsg rtos initialisation and de-initialisation