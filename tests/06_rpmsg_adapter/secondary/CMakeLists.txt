# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_06_rpmsg_adapter_secondary_core LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH examples/_boards/${board}/multicore_examples/rpmsg_lite_pingpong
        CUSTOM_PRJ_CONF_PATH middleware/multicore/rpmsg-lite/tests/_boards/${board}/${core_id} middleware/multicore/rpmsg-lite/tests/_boards/${board}/06_rpmsg_adapter)

include(${SdkRootDirPath}/CMakeLists.txt)

# This will point to: ${SdkRootDirPath}/examples/_boards/${board}/multicore_examples/reconfig.cmake
# where common board multicore properties are set.
include(${CMAKE_CURRENT_LIST_DIR}/../../config.cmake)
# Test-specific board reconfig (e.g. shared memory addresses, linker aliases per board)
include(${CMAKE_CURRENT_LIST_DIR}/../../_boards/${board}/06_rpmsg_adapter/reconfig.cmake OPTIONAL)

mcux_add_source(
    SOURCES main.c
)

mcux_add_source(
    SOURCES rpmsg_sh_mem_mdk.c
    TOOLCHAINS mdk
)

mcux_add_include(
    INCLUDES .
)

mcux_add_configuration(
        CC "-D__USE_SHMEM -DUNITY_NOT_PRINT_LOG -DNO_CRP \
            "
)

mcux_add_linker_symbol(
    SYMBOLS "__use_shmem__=1 \
             __multicore__=1 \
            "
)

# For GCC: create rpmsg_sh_mem_start/end aliases from linker script symbols.
# Boards that use SH_MEM_NOT_TAKEN_FROM_LINKER (e.g. wireless) have no
# __RPMSG_SH_MEM_START__ in their linker script; they set
# RPMSG_SH_MEM_FIXED_ADDR=TRUE in their cm33_core1/reconfig.cmake and add
# fixed-address defsyms there instead.
# For IAR: these are already defined in the ICF when __use_shmem__ is set.
if("${CONFIG_TOOLCHAIN}" STREQUAL "armgcc")
    if(NOT RPMSG_SH_MEM_FIXED_ADDR)
        mcux_add_linker_symbol(
            SYMBOLS "rpmsg_sh_mem_start=__RPMSG_SH_MEM_START__ \
                     rpmsg_sh_mem_end=__RPMSG_SH_MEM_END__ \
                    "
        )
    endif()
endif()

# In case GCov is enabled the stack and heap size is set in gcov component
if(NOT CONFIG_MCUX_COMPONENT_utilities.gcov)
mcux_add_linker_symbol(
    SYMBOLS "__stack_size__=0x400 \
             __heap_size__=0x2000 \
            "
)
endif()

mcux_add_configuration(
    LD "-Wl,--allow-multiple-definition"
    TOOLCHAINS armgcc
)

mcux_convert_binary(
        TOOLCHAINS armgcc mdk iar
        BINARY ${APPLICATION_BINARY_DIR}/${CONFIG_TOOLCHAIN}/core1_image.bin
)
