#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

mcux_add_source(
    SOURCES pin_mux.c
            hardware_init.c
)

mcux_add_include(
    INCLUDES .
    INCLUDES ..
)

mcux_add_xtensa_configuration(
    CC "-DXOS_CLOCK_FREQ=594000000"
)
mcux_add_custom_command(
  TOOLCHAINS xtensa
  BUILD_EVENT POST_BUILD
  BUILD_COMMAND ${CMAKE_OBJCOPY}
  --xtensa-params= -Obinary ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.elf ${APPLICATION_BINARY_DIR}/dsp_literal_release.bin
  --xtensa-core=${XTENSA_CORE}
  --xtensa-system=${XTENSA_SYSTEM}
  --only-section=.Level3InterruptVector.literal
  --only-section=.DebugExceptionVector.literal
  --only-section=.NMIExceptionVector.literal
  --only-section=.UserExceptionVector.literal
)

mcux_add_custom_command(
  TOOLCHAINS xtensa
  BUILD_EVENT POST_BUILD
  BUILD_COMMAND ${CMAKE_OBJCOPY}
  --xtensa-params= -Obinary ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.elf ${APPLICATION_BINARY_DIR}/dsp_text_release.bin
  --xtensa-core=${XTENSA_CORE}
  --xtensa-system=${XTENSA_SYSTEM}
  --only-section=.ResetVector.text
  --only-section=.WindowVectors.text
  --only-section=.Level2InterruptVector.text
  --only-section=.Level3InterruptVector.text
  --only-section=.DebugExceptionVector.text
  --only-section=.NMIExceptionVector.text
  --only-section=.KernelExceptionVector.text
  --only-section=.UserExceptionVector.text
  --only-section=.DoubleExceptionVector.text
)

mcux_add_custom_command(
  TOOLCHAINS xtensa
  BUILD_EVENT POST_BUILD
  BUILD_COMMAND ${CMAKE_OBJCOPY}
  --xtensa-params= -Obinary ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.elf ${APPLICATION_BINARY_DIR}/dsp_data_release.bin
  --xtensa-core=${XTENSA_CORE}
  --xtensa-system=${XTENSA_SYSTEM}
  --only-section=.clib.rodata
  --only-section=.rtos.rodata
  --only-section=.rodata
  --only-section=.text
  --only-section=.clib.data
  --only-section=.rtos.percpu.data
  --only-section=.data
  --only-section=.bss
)

mcux_add_custom_command(
  TOOLCHAINS xtensa
  BUILD_EVENT POST_BUILD
  BUILD_COMMAND ${CMAKE_OBJCOPY}
  --xtensa-params= -Obinary ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.elf ${APPLICATION_BINARY_DIR}/dsp_ncache_release.bin
  --xtensa-core=${XTENSA_CORE}
  --xtensa-system=${XTENSA_SYSTEM}
  --only-section=NonCacheable
)

mcux_add_custom_command(
  TOOLCHAINS xtensa
  BUILD_EVENT POST_BUILD
  BUILD_COMMAND ${CMAKE_OBJCOPY}
  --xtensa-params= -Obinary ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.elf ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin
)
