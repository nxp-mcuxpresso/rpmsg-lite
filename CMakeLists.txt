#
# Copyright 2024,2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause
#

# middleware.multicore.rpmsg-lite
if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite)
    mcux_component_version(5.2.1)
    mcux_add_include(
        INCLUDES lib/include
    )

    mcux_add_source(
        SOURCES lib/include/llist.h
                lib/include/rpmsg_compiler.h
                lib/include/rpmsg_default_config.h
                lib/include/rpmsg_env.h
                lib/include/rpmsg_lite.h
                lib/include/virtio_ring.h
                lib/include/virtqueue.h
    )

    mcux_add_source(
        SOURCES lib/common/llist.c
                lib/rpmsg_lite/rpmsg_lite.c
                lib/virtio/virtqueue.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.queue)
    mcux_add_source(
        SOURCES lib/include/rpmsg_queue.h
    )

    mcux_add_source(
        SOURCES lib/rpmsg_lite/rpmsg_queue.c
    )
endif()

if (CONFIG_MCUX_COMPONENT_middleware.multicore.rpmsg-lite.ns)
    mcux_add_source(
        SOURCES lib/include/rpmsg_ns.h
    )

    mcux_add_source(
        SOURCES lib/rpmsg_lite/rpmsg_ns.c
    )
endif()

if (CONFIG_MCUX_PRJSEG_middleware.multicore.rpmsg-lite.generated_config)
    # Left intentionaly empty - For ecosystem data generation
endif()

include(cmake/env.cmake)
include(cmake/platform.cmake)
