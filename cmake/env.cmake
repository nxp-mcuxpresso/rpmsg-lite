#
# Copyright 2024 NXP
#
# SPDX-License-Identifier: BSD-3-Clause
#

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.bm)
    mcux_add_include(
        INCLUDES ../lib/include/environment/bm/
    )

    mcux_add_source(
        SOURCES ../lib/include/environment/bm/rpmsg_env_specific.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/environment/rpmsg_env_bm.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.freertos)
    mcux_add_include(
        INCLUDES ../lib/include/environment/freertos/
    )

    mcux_add_source(
        SOURCES ../lib/include/environment/freertos/rpmsg_env_specific.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/environment/rpmsg_env_freertos.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.qnx)
    mcux_add_include(
        INCLUDES ../lib/include/environment/qnx/
    )

    mcux_add_source(
        SOURCES ../lib/include/environment/qnx/rpmsg_env_specific.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/environment/rpmsg_env_qnx.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.threadx)
    mcux_add_include(
        INCLUDES ../lib/include/environment/threadx/
    )

    mcux_add_source(
        SOURCES ../lib/include/environment/threadx/rpmsg_env_specific.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/environment/rpmsg_env_threadx.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.xos)
    mcux_add_include(
        INCLUDES ../lib/include/environment/xos/
    )

    mcux_add_source(
        SOURCES ../lib/include/environment/xos/rpmsg_env_specific.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/environment/rpmsg_env_xos.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.zephyr)
    mcux_add_include(
        INCLUDES ../lib/include/environment/zephyr/
    )

    mcux_add_source(
        SOURCES ../lib/include/environment/zephyr/rpmsg_env_specific.h
    )

    mcux_add_source(
        SOURCES ../lib/rpmsg_lite/porting/environment/rpmsg_env_zephyr.c
    )
endif()
