#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

ExternalZephyrProject_Add(
        APPLICATION test_02_epts_channels_dsp_fusionf1
        SOURCE_DIR  ${APP_DIR}/../fusionf1
        board ${SB_CONFIG_dsp_board}
        core_id ${SB_CONFIG_dsp_core_id}
        config ${SB_CONFIG_dsp_config}
        toolchain ${SB_CONFIG_dsp_toolchain}
)

# Let's build the fusionf1 application first
add_dependencies(${DEFAULT_IMAGE} test_02_epts_channels_dsp_fusionf1)
