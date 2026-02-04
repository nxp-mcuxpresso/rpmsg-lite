# Copyright 2023-2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.20.0)

set(REMOTE_ZEPHYR_DIR ${CMAKE_CURRENT_BINARY_DIR}/../remote/zephyr)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rpmsglite_pingpong)

message(STATUS "${BOARD} compile as Main Core in this sample")

if(CONFIG_INCLUDE_REMOTE_DIR)
  target_include_directories(zephyr_interface
    INTERFACE ${REMOTE_ZEPHYR_DIR}/include/public)
endif()

enable_language(C ASM)

target_sources(app PRIVATE src/main.c)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Also include this directory into rpmsg_lite library because of project specific include rpsmg_config.h
target_include_directories(rpmsg_lite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (CONFIG_BOARD_MIMXRT685_EVK OR CONFIG_BOARD_MIMXRT700_EVK)
    include("${ZEPHYR_BASE}/samples/boards/nxp/adsp/rtxxx/common/dsp-load.cmake")
endif()
