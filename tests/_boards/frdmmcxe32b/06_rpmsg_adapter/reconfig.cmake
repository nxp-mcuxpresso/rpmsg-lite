#
# Copyright 2026 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

# MCXE32B (dual Cortex-M7) secondary core (M7_1) boot address.
#
# The 06_rpmsg_adapter test drives the whole COREUP/READY/EP_READY handshake
# from HAL_RpmsgMcmgrMasterInit(), which releases the secondary core with:
#
#     MCMGR_StartCore(kMCMGR_Core1, (void *)REMOTE_CORE_BOOT_ADDRESS, ...)
#
# On MCXE32B the mcmgr porting layer programs this value straight into
# MC_ME->PRTN0_CORE1_ADDR, and the released M7_1 fetches its initial SP and
# reset vector from there. It MUST match the flash window where the embedded
# secondary image is linked: m_core1_image_start = 0x005C0000 (see
# devices/MCX/MCXE/MCXE32B/gcc/MCXE32B_cm7_core0_flash.ld and the matching
# CORE1_BOOT_ADDRESS 0x005C0000 used by the frdmmcxe32b driver examples).
#
# Without this override, REMOTE_CORE_BOOT_ADDRESS falls back to the generic
# header default 0x01000000 (components/rpmsg/fsl_adapter_rpmsg.h). That
# address is not a valid code/vector region on MCXE32B: M7_1 comes out of
# reset with VTOR = 0x01000000, the first MU2_B RX interrupt fetches its
# handler from an unreadable region (CFSR IACCVIOL), the fault escalates,
# the HardFault vector fetch also fails (HFSR VECTTBL) and the core locks up.
# The primary then never sees READY and every test case times out.
mcux_add_configuration(
    CC "-DREMOTE_CORE_BOOT_ADDRESS=0x005C0000U"
)
