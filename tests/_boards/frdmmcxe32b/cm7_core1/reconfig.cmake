#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# Activate the RPMSG shared-memory region on the secondary core as well.
# The region lives at m_stdby_ram_start = 0x20400000 (size 0x1800).
mcux_add_linker_symbol(
    SYMBOLS "__use_shmem__=1"
)
