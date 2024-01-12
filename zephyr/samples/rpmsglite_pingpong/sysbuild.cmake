# Copyright 2022-2023 NXP
#
# SPDX-License-Identifier: Apache-2.0

# Add external project
ExternalZephyrProject_Add(
    APPLICATION rpmsglite_remote
    SOURCE_DIR ${APP_DIR}/remote
    BOARD ${SB_CONFIG_RPMSG_LITE_REMOTE_BOARD}
  )

# Add dependencies so that the remote sample will be built first
# This is required because some primary cores need information from the
# remote core's build, such as the output image's LMA
add_dependencies(rpmsglite_pingpong rpmsglite_remote)
sysbuild_add_dependencies(CONFIGURE rpmsglite_pingpong rpmsglite_remote)

if(SB_CONFIG_BOOTLOADER_MCUBOOT)
  # Make sure MCUboot is flashed first
  sysbuild_add_dependencies(FLASH rpmsglite_remote mcuboot)
endif()
