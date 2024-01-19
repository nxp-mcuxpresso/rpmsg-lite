# RPMSG-Lite Zephyr support

## Adding RPMSG-Lite to Zephyr as module

To include RPMSG-Lite Zephyr, update Zephyr's west.yml:

``` yml
  projects:
    - name: rpmsg-lite
      revision: <revision with Zephyr support>
      url: <rpmsg-lite repository>
      path: modules/lib/rpmsg-lite
```
Then run:
``` sh
west update rpmsg-lite
```

## Supported Platforms

The RPSMG-Lite is currently supported on these platforms:

 * lpc5411x
 * lpc55s69
 * imxrt1170
 * imxrt1160

To support other platform please create new platform specific file `rpmsg_platform_zephyr_ipm.c` in
directory **`rpmsg-lite/lib/rpmsg_lite/porting/platform/<platform_name>`** and add new entry in file `rpmsg-lite/zephyr/CMakeLists.txt`.
```cmake
  if(("${MCUX_DEVICE}" STREQUAL "LPC54114") OR ("${MCUX_DEVICE}" STREQUAL "LPC54114_m0"))
    set(RPMSG_LITE_PLATFORM_PATH "lpc5411x")
  ...
  ...
  elseif("${MCUX_DEVICE}" STREQUAL "MIMXRT1176_cm4" OR "${MCUX_DEVICE}" STREQUAL "MIMXRT1176_cm7")
    set(RPMSG_LITE_PLATFORM_PATH "imxrt1170")
```
Also update samples with board/platform specific configs or DT overlays in: `rpmsg-lite/zephyr/samples/<sample_name>/boards` and
`rpmsg-lite/zephyr/samples/<sample_name>/remote/boards`

## RPMSG-Lite module files

- *rpmsg-lite/zephyr/* - This directory holds all files required for Zephyr module
- *rpmsg-lite/zephyr/module.yml* - This file defines module name and Cmake and Kconfig location
- *rpmsg-lite/zephyr/CMakeList.txt* - Defines module's includes and source files base on Kconfig
- *rpmsg-lite/zephyr/Kconfig* - Defines RPMSG-Lite module configuration

### Kconfig

Kconfig options:

- **CONFIG_RPMSGLITE** - enable RPMSG-Lite support
- **CONFIG_RPMSGLITE_QUEUE** - enable RPMSG-Lite Queue support
- **CONFIG_RPMSGLITE_QUEUE** - enable RPMSG-Lite Name Service support

### RPMSG-Lite User/Project Configuration

The RPMSG-Lite Library can be further configured by `rpmsg_config.h` header file.

This header file needs to be included per application project.

File can be found in `rpmsg-lite/zephyr/samples/rpmsglite_pingpong/rpmsg_config.h`.

User can copy and update this config file.

### CMakeList

CMakeLists.txt defines RPSMG-Lite as Zephyr library. Includes all required directories and source files for Zephyr environment.

## Samples

Samples are located in *zephyr/samples*. Examples can be build by west tool:

### RPMSG-Lite Ping Pong Example

```sh
cd path/to/zephyrproject
west build --sysbuild -b <supported board> modules/lib/rpmsg-lite/zephyr/samples/rpmsglite_pingpong/ -p
west flash
```
