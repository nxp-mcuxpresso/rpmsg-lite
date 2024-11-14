#
# Copyright 2024 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

include(${SdkRootDirPath}/examples/_boards/${board}/multicore_examples/reconfig.cmake OPTIONAL)
include(${CMAKE_CURRENT_LIST_DIR}/_boards/${board}/${core_id}/reconfig.cmake OPTIONAL)

mcux_add_source(
    SOURCES core_util.c
)

mcux_add_include(
    INCLUDES .
)
