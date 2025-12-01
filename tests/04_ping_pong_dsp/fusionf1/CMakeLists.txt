#
# Copyright 2024-2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_04_ping_pong_dsp_fusionf1 LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH examples/_boards/${board}/multicore_examples/rpmsg_lite_pingpong 
        CUSTOM_PRJ_CONF_PATH middleware/multicore/rpmsg-lite/tests/_boards/${board}/dsp middleware/multicore/rpmsg-lite/tests/_boards/${board}/dsp/${core_id} middleware/multicore/rpmsg-lite/tests/_boards/${board}/dsp/04_ping_pong_dsp)

include(${SdkRootDirPath}/CMakeLists.txt)

# This will point to: ${SdkRootDirPath}/examples/_boards/${board}/multicore_examples/reconfig.cmake
# where common board multicore properties are set.
include(${CMAKE_CURRENT_LIST_DIR}/../../config.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../_boards/${board}/dsp/${core_id}/reconfig.cmake OPTIONAL)

mcux_add_source(
    SOURCES main.c
)

mcux_add_include(
    INCLUDES .
             ../
)

mcux_add_configuration(
        CC "-D__USE_SHMEM -DUNITY_NOT_PRINT_LOG -DNO_CRP -DFSL_RTOS_XOS -DTEST_PROTECT_NOT_IMPLEMENTED"
)

#mcux_add_linker_symbol(
#    SYMBOLS "__use_shmem__=1 \
#             __multicore__=1 \
#            "
#)

# In case GCov is enabled the stack and heap size is set in gcov component
if(NOT CONFIG_MCUX_COMPONENT_utilities.gcov)
#mcux_add_linker_symbol(
#    SYMBOLS "__stack_size__=0x400 \
#             __heap_size__=0x2000 \
#            "
#)
endif()

