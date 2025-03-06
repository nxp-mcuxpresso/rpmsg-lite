# Changelog RPMSG-Lite

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Add MCXL20 porting layer and unit testing
- New utility macro `RL_CALCULATE_BUFFER_COUNT_DOWN_SAFE` to safely determine maximum buffer count within shared memory while preventing integer underflow.
- RT700 platform add support for MCMGR in DSPs

### Changed

- Change `rpmsg_platform.c` to support new MCMGR API
- Improved input validation in initialization functions to properly handle insufficient memory size conditions.
- Refactored repeated buffer count calculation pattern for better code maintainability.
- To make sure that remote has already registered IRQ there is required App level IPC mechanism to notify master about it

### Fixed

- Fixed CERT-C INT31-C violation by adding compile-time check to ensure `RL_PLATFORM_HIGHEST_LINK_ID` remains within safe range for 16-bit casting in virtqueue ID creation.
- Fixed CERT-C INT30-C violations by adding protection against unsigned integer underflow in shared memory calculations, specifically in `shmem_length - (uint32_t)RL_VRING_OVERHEAD` and `shmem_length - 2U * shmem_config.vring_size` expressions.
- Fixed CERT INT31-C violation in `platform_interrupt_disable()` and similar functions by replacing unsafe cast from `uint32_t` to `int32_t` with a return of `0` constant.
- Fixed unsigned integer underflow in `rpmsg_lite_alloc_tx_buffer()` where subtracting header size from buffer size could wrap around if buffer was too small, potentially leading to incorrect buffer sizing.
- Fixed CERT-C INT31-C violation in `rpmsg_lite.c` where `size` parameter was cast from `uint32_t` to `uint16_t` without proper validation.
  - Applied consistent masking approach to both `size` and `flags` parameters: `(uint16_t)(value & 0xFFFFU)`.
  - This fix prevents potential data loss when size values exceed 65535.
- Fixed CERT INT31-C violation in `env_memset` functions by explicitly converting `int32_t` values to unsigned char using bit masking. This prevents potential data loss or misinterpretation when passing values outside the unsigned char range (0-255) to the standard memset() function.
- Fixed CERT-C INT31-C violations in RPMsg-Lite environment porting: Added validation checks for signed-to-unsigned integer conversions to prevent data loss and misinterpretation.
  - `rpmsg_env_freertos.c`: Added validation before converting int32_t to UBaseType_t.
  - `rpmsg_env_qnx.c`: Fixed format string and added validation before assigning to mqstat fields.
  - `rpmsg_env_threadx.c`: Added validation to prevent integer overflow and negative values.
  - `rpmsg_env_xos.c`: Added range checking before casting to uint16_t.
  - `rpmsg_env_zephyr.c`: Added validation before passing values to k_msgq_init.
- Fixed a CERT INT31-C compliance issue in `env_get_current_queue_size()` function where an unsigned queue count was cast to a signed int32_t without proper validation, which could lead to lost or misinterpreted data if queue size exceeded INT32_MAX.
- Fixed CERT INT31-C violation in `rpmsg_platform.c` where `memcmp()` return value (signed int) was compared with unsigned constant without proper type handling.
- Fixed CERT INT31-C violation in `rpmsg_platform.c` where casting from uint32_t to uint16_t could potentially result in data loss. Changed length variable type from uint16_t to uint32_t to properly handle memory address differences without truncation.
- Fixed potential integer overflow in `env_sleep_msec()` function in ThreadX environment implementation by rearranging calculation order in the sleep duration formula.
- Fixed CERT-C INT31-C violation in RPMsg-Lite where bitwise NOT operations on integer constants were performed in signed integer context before being cast to unsigned. This could potentially lead to misinterpreted data on `imx943` platform.
- Added RL_MAX_BUFFER_COUNT (32768U) and RL_MAX_VRING_ALIGN (65536U) limit to ensure alignment values cannot contribute to integer overflow
- Fixed CERT INT31-C violation in vring_need_event(), added cast to `uint16_t` for each operand.

## [5.1.4] - 27-Mar-2025

### Added

- Add KW43B43 porting layer

### Changed

- Doxygen bump to version 1.9.6

## [5.1.3] - 13-Jan-2025

### Added

- Memory cache management of shared memory. Enable with `#define RL_USE_DCACHE (1)` in `rpmsg_config.h` in case of data cache is used.
- Cmake/Kconfig support added.
- Porting layers for imx95, imxrt700, mcmxw71x, mcmxw72x, kw47b42 added.

## [5.1.2] - 08-Jul-2024

### Changed

- Zephyr-related changes.
- Minor Misra corrections.

## [5.1.1] - 19-Jan-2024

### Added

- Test suite provided.
- Zephyr support added.

### Changed

- Minor changes in platform and env. layers, minor test code updates.

## [5.1.0] - 02-Aug-2023

### Added

- RPMsg-Lite: Added aarch64 support.

### Changed

- RPMsg-Lite: Increased the queue size to (2 * RL_BUFFER_COUNT) to cover zero copy cases.
- Code formatting using LLVM16.

### Fixed

- Resolved issues in ThreadX env. layer implementation.

## [5.0.0] - 19-Jan-2023

### Added

- Timeout parameter added to rpmsg_lite_wait_for_link_up API function.

### Changed

- Improved debug check buffers implementation - instead of checking the pointer fits into shared memory check the presence in the VirtIO ring descriptors list.
- VRING_SIZE is set based on number of used buffers now (as calculated in vring_init) - updated for all platforms that are not communicating to Linux rpmsg counterpart.

### Fixed

- Fixed wrong RL_VRING_OVERHEAD macro comment in platform.h files
- Misra corrections.

## [4.0.0] - 20-Jun-2022

### Added

- Added support for custom shared memory arrangement per the RPMsg_Lite instance.
- Introduced new rpmsg_lite_wait_for_link_up() API function - this allows to avoid using busy loops in rtos environments, GitHub PR [#21](https://github.com/nxp-mcuxpresso/rpmsg-lite/pull/21).

### Changed

- Adjusted rpmsg_lite_is_link_up() to return RL_TRUE/RL_FALSE.

## [3.2.0] - 17-Jan-2022

### Added

- Added support for i.MX8 MP multicore platform.

### Changed

- Improved static allocations - allow OS-specific objects being allocated statically, GitHub PR [#14](https://github.com/nxp-mcuxpresso/rpmsg-lite/pull/14).
- Aligned rpmsg_env_xos.c and some platform layers to latest static allocation support.

### Fixed

- Minor Misra and typo corrections, GitHub PR [#19](https://github.com/nxp-mcuxpresso/rpmsg-lite/pull/19), [#20](https://github.com/nxp-mcuxpresso/rpmsg-lite/pull/20).

## [3.1.2] - 16-Jul-2021

### Added

- Addressed MISRA 21.6 rule violation in rpmsg_env.h (use SDK's PRINTF in MCUXpressoSDK examples, otherwise stdio printf is used).
- Added environment layers for XOS.
- Added support for i.MX RT500, i.MX RT1160 and i.MX RT1170 multicore platforms.

### Fixed

- Fixed incorrect description of the rpmsg_lite_get_endpoint_from_addr function.

### Changed

- Updated RL_BUFFER_COUNT documentation (issue [#10](https://github.com/nxp-mcuxpresso/rpmsg-lite/issues/10)).
- Updated imxrt600_hifi4 platform layer.

## [3.1.1] - 15-Jan-2021

### Added

- Introduced RL_ALLOW_CONSUMED_BUFFERS_NOTIFICATION config option to allow opposite side notification sending each time received buffers are consumed and put into the queue of available buffers.
- Added environment layers for Threadx.
- Added support for i.MX8QM multicore platform.

### Changed

- Several MISRA C-2012 violations addressed.

## [3.1.0] - 22-Jul-2020

### Added

- Added support for several new multicore platforms.

### Fixed

- MISRA C-2012 violations fixed (7.4).
- Fixed missing lock in rpmsg_lite_rx_callback() for QNX env.
- Correction of rpmsg_lite_instance structure members description.
- Address -Waddress-of-packed-member warnings in GCC9.

### Changed

- Clang update to v10.0.0, code re-formatted.

## [3.0.0] - 20-Dec-2019

### Added

- Added support for several new multicore platforms.

### Fixed

- MISRA C-2012 violations fixed, incl. data types consolidation.
- Code formatted.

## [2.2.0] - 20-Mar-2019

### Added

- Added configuration macro RL_DEBUG_CHECK_BUFFERS.
- Several MISRA violations fixed.
- Added environment layers for QNX and Zephyr.
- Allow environment context required for some environment (controlled by the RL_USE_ENVIRONMENT_CONTEXT configuration macro).
- Data types consolidation.

## [1.1.0] - 28-Apr-2017

### Added

- Supporting i.MX6SX and i.MX7D MPU platforms.
- Supporting LPC5411x MCU platform.
- Baremental and FreeRTOS support.
- Support of copy and zero-copy transfer.
- Support of static API (without dynamic allocations).


[unreleased]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v5.1.4...HEAD
[5.1.4]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v5.1.3...v5.1.4
[5.1.3]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v5.1.2...v5.1.3
[5.1.2]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v5.1.1...v5.1.2
[5.1.1]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v5.1.0...v5.1.1
[5.1.0]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v5.0.0...v5.1.0
[5.0.0]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v4.0.0...v5.0.0
[4.0.0]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v3.2.0...v4.0.0
[3.2.0]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v3.1.2...v3.2.0
[3.1.2]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v3.1.1...v3.1.2
[3.1.1]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v3.1.0...v3.1.1
[3.1.0]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v3.0.0...v3.1.0
[3.0.0]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v2.2.0...v3.0.0
[2.2.0]: https://github.com/nxp-mcuxpresso/rpmsg-lite/compare/v1.1.0...v2.2.0
[1.1.0]: https://github.com/nxp-mcuxpresso/rpmsg-lite/releases/tag/v1.1.0
