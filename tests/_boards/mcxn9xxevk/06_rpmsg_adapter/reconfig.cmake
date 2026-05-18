#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# MCXN9xx core1 boot address in SRAM
mcux_add_configuration(
    CC "-DREMOTE_CORE_BOOT_ADDRESS=0x2004E000U"
)
