#
# Copyright 2024-2025 NXP
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

if(CONFIG_MCUX_COMPONENT_utilities.gcov)
    # Get all source files in the directory
    file(GLOB_RECURSE RPMSG_LITE_SOURCES "${SdkRootDirPath}/middleware/multicore/rpmsg-lite/lib/*.c")

    # Set properties for all those files
    foreach(SOURCE_FILE ${RPMSG_LITE_SOURCES})
        message("GCOV: Adding coverage flags for ${SOURCE_FILE}")
        set_source_files_properties(${SOURCE_FILE} PROPERTIES
            COMPILE_FLAGS "-g3 -ftest-coverage -fprofile-arcs -fkeep-inline-functions -fkeep-static-functions")
    endforeach()
endif()
