# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(test_06_rpmsg_adapter_primary_core LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH examples/_boards/${board}/multicore_examples/rpmsg_lite_pingpong
        CUSTOM_PRJ_CONF_PATH middleware/multicore/rpmsg-lite/tests/_boards/${board}/${core_id} middleware/multicore/rpmsg-lite/tests/_boards/${board}/06_rpmsg_adapter)

include(${SdkRootDirPath}/CMakeLists.txt)

# This will point to: ${SdkRootDirPath}/examples/_boards/${board}/multicore_examples/reconfig.cmake
# where common board multicore properties are set.
include(${CMAKE_CURRENT_LIST_DIR}/../../config.cmake)
include(${SdkRootDirPath}/examples/_boards/${board}/multicore_examples/rpmsg_lite_pingpong/${core_id}/reconfig.cmake OPTIONAL)
# Test-specific board reconfig (e.g. REMOTE_CORE_BOOT_ADDRESS per board)
include(${CMAKE_CURRENT_LIST_DIR}/../../_boards/${board}/06_rpmsg_adapter/reconfig.cmake OPTIONAL)


mcux_add_source(
    SOURCES main.c
)

mcux_add_include(
    INCLUDES .
)

mcux_add_configuration(
        CC "-D__USE_SHMEM -DUNITY_DUMP_RESULT -DUNITY_CUSTOM_OUTPUT_CHAR -DUNITY_DUMP_CASE_RESULT_ONLY"
)

mcux_add_linker_symbol(
    SYMBOLS "__use_shmem__=1 \
             __multicore__=1 \
            "
)

# For GCC: create rpmsg_sh_mem_start/end aliases from linker script symbols.
# For IAR: these are already defined in the ICF when __use_shmem__ is set.
if("${CONFIG_TOOLCHAIN}" STREQUAL "armgcc")
mcux_add_linker_symbol(
    SYMBOLS "rpmsg_sh_mem_start=__RPMSG_SH_MEM_START__ \
             rpmsg_sh_mem_end=__RPMSG_SH_MEM_END__ \
            "
)
endif()

# In case GCov is enabled the stack and heap size is set in gcov component
if(NOT CONFIG_MCUX_COMPONENT_utilities.gcov)
mcux_add_linker_symbol(
    SYMBOLS "__stack_size__=0x400 \
             __heap_size__=0x2000 \
            "
)
endif()

mcux_add_iar_configuration(
    LD "--image_input=${APPLICATION_BINARY_DIR}/../test_06_rpmsg_adapter_secondary_core/iar/core1_image.bin,_core1_image,__core1_image,4 \
        --keep _core1_image"
)

mcux_add_mdk_configuration(
    LD "--library_type=standardlib --keep=*(*core1_code)"
)

mcux_add_include(
        TOOLCHAINS mdk armgcc
        BASE_PATH ${APPLICATION_BINARY_DIR}
        INCLUDES ../test_06_rpmsg_adapter_secondary_core/${CONFIG_TOOLCHAIN}/
)
