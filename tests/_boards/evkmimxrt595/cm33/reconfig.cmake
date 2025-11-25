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
    INCLUDES ..
)
