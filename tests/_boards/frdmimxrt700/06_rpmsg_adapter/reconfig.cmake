#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# RT700 core1 (CM33_CORE1) boot address in SRAM
mcux_add_configuration(
    CC "-DREMOTE_CORE_BOOT_ADDRESS=0x20600000U"
)
