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
    CC "-DXOS_CLOCK_FREQ=198000000"
)
mcux_add_custom_command(
  TOOLCHAINS xtensa
  BUILD_EVENT POST_BUILD
  BUILD_COMMAND ${CMAKE_OBJCOPY}
  --xtensa-params= -Obinary ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.elf ${APPLICATION_BINARY_DIR}/dsp_reset_release.bin
  --xtensa-core=${XTENSA_CORE}
  --xtensa-system=${XTENSA_SYSTEM}
  --only-section=.ResetVector.text
)

mcux_add_custom_command(
  TOOLCHAINS xtensa
  BUILD_EVENT POST_BUILD
  BUILD_COMMAND ${CMAKE_OBJCOPY}
  --xtensa-params= -Obinary ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.elf ${APPLICATION_BINARY_DIR}/dsp_text_release.bin
  --xtensa-core=${XTENSA_CORE}
  --xtensa-system=${XTENSA_SYSTEM}
  --only-section=.WindowVectors.text
  --only-section=.Level2InterruptVector.text
  --only-section=.Level3InterruptVector.literal
  --only-section=.Level3InterruptVector.text
  --only-section=.DebugExceptionVector.literal
  --only-section=.DebugExceptionVector.text
  --only-section=.NMIExceptionVector.literal
  --only-section=.NMIExceptionVector.text
  --only-section=.KernelExceptionVector.text
  --only-section=.UserExceptionVector.text
  --only-section=.DoubleExceptionVector.text
  --only-section=.text
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
  --only-section=.clib.data
  --only-section=.rtos.percpu.data
  --only-section=.data
  --only-section=.bss
)

mcux_add_custom_command(
  TOOLCHAINS xtensa
  BUILD_EVENT POST_BUILD
  BUILD_COMMAND ${CMAKE_OBJCOPY}
  --xtensa-params= -Obinary ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.elf ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin
)
