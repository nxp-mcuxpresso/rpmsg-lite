#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

mcux_add_configuration(
    CC "-DSH_MEM_NOT_TAKEN_FROM_LINKER -DRPMSG_LITE_SHMEM_BASE=0xB0008800"
)
