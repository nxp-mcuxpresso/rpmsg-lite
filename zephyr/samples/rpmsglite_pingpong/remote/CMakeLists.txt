cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rpmsglite_remote)
message(STATUS "${BOARD} compiles as Remote Core in this sample")

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Also include this directory into rpmsg_lite library because of project specific include rpsmg_config.h
target_include_directories(rpmsg_lite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
