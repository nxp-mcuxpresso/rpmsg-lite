#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# Generate Master Boot Image with TZM data
function(mcux_convert_binary)
  # "Overload" mcux_convert_binary from generic secondary core CMakeLists.txt
  # Intentionally disabled for this project: nxpimage produces the final bin
endfunction()
mcux_add_custom_command(
    BUILD_EVENT POST_BUILD
    BYPRODUCTS
    ${APPLICATION_BINARY_DIR}/${CONFIG_TOOLCHAIN}/core1_image.bin
    BUILD_COMMAND
    nxpimage -v mbi export
    -c ${SdkRootDirPath}/${board_root}/${board}/wireless_examples/mbi_config.yaml
    -oc inputImageFile="${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.elf"
    -oc masterBootOutputFile="${APPLICATION_BINARY_DIR}/${CONFIG_TOOLCHAIN}/core1_image.bin"
)
