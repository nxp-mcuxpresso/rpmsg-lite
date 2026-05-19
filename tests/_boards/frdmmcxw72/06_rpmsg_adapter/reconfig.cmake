#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# MCXW72x core1 boot address (NBU core in flash)
mcux_add_configuration(
    CC "-DREMOTE_CORE_BOOT_ADDRESS=0x48800000U"
)

# NBU core1 on MCXW72 has a longer boot sequence via IMU transport.
# Give secondary 3x the default timeout to receive the COREUP handshake.
mcux_add_configuration(
    CC "-DRPMSG_REMOTE_READY_RETRY_COUNT=30000000U"
)

# NBU (core1) runs at ~64 MHz — 10M wait_flag iterations would be only ~470 ms,
# not enough for the copy-send round-trip through IMU.  Use 50M (~2.3 s) instead.
mcux_add_configuration(
    CC "-DTC_WAIT_RETRY_COUNT=50000000U"
)

# On FRDM-MCXW72 the NBU (core1) is still completing rpmsg_lite_remote_init()
# when primary receives the READY event.  Without a brief delay the master_init
# link-up kick and the first HAL_RpmsgSend kick arrive back-to-back in the
# IMU FIFO; the NBU may coalesce them into one ISR, dropping the first
# received message.  5 ms is enough for the ~64 MHz NBU to finish init.
mcux_add_configuration(
    CC "-DRPMSG_MASTER_INIT_DELAY_MS=5U"
)

# Secondary core (cm33_core1) needs SH_MEM_NOT_TAKEN_FROM_LINKER because
# the MCXW72 core1 linker scripts do not export rpmsg shared-memory symbols
# in the standard way.  Also provide IAR define_symbol fallback.
if("${core_id}" STREQUAL "cm33_core1")
mcux_add_configuration(
    CC "-DSH_MEM_NOT_TAKEN_FROM_LINKER"
)

# Suppress the __RPMSG_SH_MEM_START__ alias defsym in secondary/CMakeLists.txt
# (wireless core1 linker scripts have no such symbol) and provide fixed addresses.
set(RPMSG_SH_MEM_FIXED_ADDR TRUE CACHE BOOL "" FORCE)

if("${CONFIG_TOOLCHAIN}" STREQUAL "armgcc")
    mcux_add_linker_symbol(
        SYMBOLS "rpmsg_sh_mem_start=0xB0008800 \
                 rpmsg_sh_mem_end=0xB000A000 \
                "
    )
endif()

# For IAR: The core1 ICF does not export rpmsg_sh_mem_start/end.
# Use --define_symbol to make them available as public linker symbols
# that fsl_adapter_rpmsg.c can resolve.
if("${CONFIG_TOOLCHAIN}" STREQUAL "iar")
mcux_add_configuration(
    LD "--define_symbol rpmsg_sh_mem_start=0xB0008800 --define_symbol rpmsg_sh_mem_end=0xB000A000"
)
endif()
endif()
