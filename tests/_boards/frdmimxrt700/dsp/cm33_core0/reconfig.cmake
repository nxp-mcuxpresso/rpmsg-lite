#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

mcux_add_source(
    SOURCES pin_mux.c
            hardware_init.c
)

mcux_add_include(
    INCLUDES .
)

mcux_remove_armgcc_configuration(
        CC "-DCORE1_IMAGE_COPY_TO_RAM"
)

mcux_remove_iar_configuration(
        CC "-DCORE1_IMAGE_COPY_TO_RAM"
)
