#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

ExternalZephyrProject_Add(
        APPLICATION test_03_send_receive_dsp_hifi4
        SOURCE_DIR  ${APP_DIR}/../hifi4
        board ${SB_CONFIG_dsp_board}
        core_id ${SB_CONFIG_primary_dsp_core_id}
        config ${SB_CONFIG_dsp_config}
        toolchain ${SB_CONFIG_dsp_toolchain}
)

# Let's build the hifi4 application first
add_dependencies(${DEFAULT_IMAGE} test_03_send_receive_dsp_hifi4)
