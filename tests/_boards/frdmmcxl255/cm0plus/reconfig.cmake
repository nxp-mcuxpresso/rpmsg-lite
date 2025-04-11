#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# Override heap size
mcux_remove_linker_symbol(
    SYMBOLS "__heap_size__=0x2000"
)
mcux_add_linker_symbol(
    SYMBOLS "__heap_size__=0x600"
)
