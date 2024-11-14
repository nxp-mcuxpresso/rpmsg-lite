#
# Copyright 2024 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_04_ping_pong_rtos_primary_core LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH examples/_boards/${board}/multicore_examples/rpmsg_lite_pingpong_rtos CUSTOM_PRJ_CONF_PATH middleware/multicore/rpmsg-lite/tests/_boards/${board}/${core_id})

include(${SdkRootDirPath}/CMakeLists.txt)

# This will point to: ${SdkRootDirPath}/examples/_boards/${board}/multicore_examples/reconfig.cmake
# where common board multicore properties are set.
include(${CMAKE_CURRENT_LIST_DIR}/../../config.cmake)
include(${SdkRootDirPath}/examples/_boards/${board}/multicore_examples/rpmsg_lite_pingpong/${core_id}/reconfig.cmake OPTIONAL)


mcux_add_source(
    SOURCES main.c
)

mcux_add_include(
    INCLUDES .
             ../
)

mcux_add_configuration(
        CC "-D__USE_SHMEM -DUNITY_DUMP_RESULT -DUNITY_CUSTOM_OUTPUT_CHAR -DUNITY_DUMP_CASE_RESULT_ONLY"
)

mcux_add_linker_symbol(
    SYMBOLS "__stack_size__=0x400 \
             __heap_size__=0x2000 \
             __use_shmem__=1 \
             __multicore__=1 \
            "
)

mcux_add_iar_configuration(
    LD "--image_input=${APPLICATION_BINARY_DIR}/../test_04_ping_pong_rtos_secondary_core/iar/core1_image.bin,_core1_image,__core1_image,4 \
        --keep _core1_image"
)

mcux_add_mdk_configuration(
    LD "--library_type=standardlib --keep=*(*core1_code)"
)

mcux_add_include(
        TOOLCHAINS mdk armgcc
        BASE_PATH ${APPLICATION_BINARY_DIR}
        INCLUDES ../test_04_ping_pong_rtos_secondary_core/${CONFIG_TOOLCHAIN}/
)
