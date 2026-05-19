#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# RT1180 core1 (CM7) boot address
mcux_add_configuration(
    CC "-DREMOTE_CORE_BOOT_ADDRESS=0x303C0000U"
)

# RT1180 secondary is cm7 at ~800 MHz.  The default RPMSG_REMOTE_READY_RETRY_COUNT
# (10 000 000) expires on the secondary in ~37 ms — before the primary (cm33 at
# ~240 MHz) has cleared ready_seen and entered the COREUP ping loop.  Secondary
# then calls rpmsg_lite_remote_init() and fires READY prematurely; primary clears
# ready_seen=0 on entry to HAL_RpmsgMcmgrMasterInit, misses the event, and
# deadlocks on reinit cycles (observed on tc_7 locally).
# 100 000 000 iterations ≈ 375 ms on cm7 — enough headroom for cm33 to reach the
# ping loop on every reinit cycle across all 7 test cases.
mcux_add_configuration(
    CC "-DRPMSG_REMOTE_READY_RETRY_COUNT=100000000U"
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
