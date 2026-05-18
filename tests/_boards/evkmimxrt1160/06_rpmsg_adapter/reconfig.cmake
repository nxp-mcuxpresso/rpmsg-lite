#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# RT1160 core1 (CM4) boot address in OCRAM
mcux_add_configuration(
    CC "-DREMOTE_CORE_BOOT_ADDRESS=0x20200000U"
)
