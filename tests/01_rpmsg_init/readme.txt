01_rpmsg_init test suite

 - baremetal project, static allocation used (#define RL_USE_STATIC_API (1) in rpmsg_config.h)

 - check multiple rpmsg initialisation and de-initialisation
 - check created default channel and state
 - check callbacks