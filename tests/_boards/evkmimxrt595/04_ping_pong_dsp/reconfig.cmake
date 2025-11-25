#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

mcux_add_include(
    BASE_PATH ${APPLICATION_BINARY_DIR}/../test_04_ping_pong_dsp_fusionf1/
    INCLUDES ./
)

mcux_add_iar_configuration(
    LD "--image_input=${APPLICATION_BINARY_DIR}/../test_04_ping_pong_dsp_fusionf1/dsp_reset_release.bin,__dsp_reset_bin,__dsp_reset_section,4\
        --keep=__dsp_reset_bin\
        --image_input=${APPLICATION_BINARY_DIR}/../test_04_ping_pong_dsp_fusionf1/dsp_text_release.bin,__dsp_text_bin,__dsp_text_section,4\
        --keep=__dsp_text_bin\
        --image_input=${APPLICATION_BINARY_DIR}/../test_04_ping_pong_dsp_fusionf1/dsp_data_release.bin,__dsp_data_bin,__dsp_data_section,4\
        --keep=__dsp_data_bin"
)
