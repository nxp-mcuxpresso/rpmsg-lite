#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# LPC55S69 core1 boot address in SRAM
mcux_add_configuration(
    CC "-DREMOTE_CORE_BOOT_ADDRESS=0x20033000U"
)
