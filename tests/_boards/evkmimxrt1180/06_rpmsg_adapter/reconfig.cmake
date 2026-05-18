#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# RT1180 core1 (CM7) boot address
mcux_add_configuration(
    CC "-DREMOTE_CORE_BOOT_ADDRESS=0x303C0000U"
)

# RT1180 linker scripts define m_rpmsg_sh_mem_start/end but not
# __RPMSG_SH_MEM_START__/__RPMSG_SH_MEM_END__.  Create the aliases
# so the generic CMakeLists defsym chain resolves correctly.
if("${CONFIG_TOOLCHAIN}" STREQUAL "armgcc")
mcux_add_linker_symbol(
    SYMBOLS "__RPMSG_SH_MEM_START__=m_rpmsg_sh_mem_start \
             __RPMSG_SH_MEM_END__=m_rpmsg_sh_mem_end \
            "
)
endif()
