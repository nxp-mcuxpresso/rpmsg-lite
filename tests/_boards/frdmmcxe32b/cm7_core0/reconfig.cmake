#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# Keep core1 image embedded in primary flash image (MDK linker).
mcux_add_mdk_configuration(
    LD "--keep=*(*core1_code)"
)

# Activate the RPMSG shared-memory region (rpmsg_sh_mem, 0x1800 bytes at
# m_stdby_ram_start = 0x20400000) defined in the MCXE32B device linker files.
mcux_add_linker_symbol(
    SYMBOLS "__use_shmem__=1"
)
