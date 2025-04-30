# Guide: Enabling and Collecting GCC Code Coverage (gcov) for Embedded Systems

## Introduction

This guide demonstrates how to implement code coverage analysis for embedded systems using GCC's `gcov` tool with semihosting support. Code coverage analysis is crucial for ensuring that tests adequately exercise the code base, helping identify untested sections.

For embedded systems, collecting coverage data presents unique challenges:
- No filesystem for storing .gcda files
- Limited debugging capabilities 
- Multi-core systems add complexity

We'll walk through implementing coverage analysis specifically for the RPMsg-Lite middleware component, but these techniques apply to any embedded project.

## 1. Building with Coverage Instrumentation

### Adding Coverage Flags

First, modify your CMake build files to add coverage instrumentation:

```cmake
set_source_files_properties("${SdkRootDirPath}/middleware/multicore/rpmsg-lite/lib" 
    PROPERTIES COMPILE_FLAGS "-g3 -ftest-coverage -fprofile-arcs -fkeep-inline-functions -fkeep-static-functions")
```

These flags enable:
- `-g3`: Debugging information
- `-ftest-coverage`: Generates .gcno files (coverage notes)
- `-fprofile-arcs`: Instruments code to track execution paths
- `-fkeep-inline-functions`: Preserves inline functions for coverage
- `-fkeep-static-functions`: Preserves static functions for coverage

### Enable Semihosting Support

For an embedded target, you need semihosting to write coverage data back to the host:

```cmake
# Ensure semihosting is enabled
add_compile_definitions(MCUX_SEGGER_SEMIHOSTING)
add_compile_definitions(GCOV_DO_COVERAGE=1)
```

This is done by including bellow config in your `prj.conf` file or as `-DCONF_FILE=`:
```kconfig
CONFIG_MCUX_COMPONENT_utilities.gcov=y
```

### Adding gcov Support Files

Include helper functions to dump coverage data:

```cmake
# Add gcov support files to the build
mcux_add_source(
    SOURCES components/unity/gcov_support.c
            components/unity/gcov_support.h
            examples_int/unit_tests/sdmmc/tools/segger_semihosting.c
)
```

This is done by including bellow config in your `prj.conf` file or as `-DCONF_FILE=`:
```kconfig
CONFIG_MCUX_COMPONENT_utilities.gcov=y
```

### Example Command to Compile Tests with Coverage

Use the following command to build tests with coverage instrumentation:

```bash
west build -b mimxrt700evk middleware/multicore/rpmsg-lite/tests/01_rpmsg_init/primary/ --sysbuild -p always --sysbuild --config flash_debug --build-dir b_11/ -- -DCONF_FILE=$(pwd)/middleware/multicore/rpmsg-lite/tests/prj-gcov-primary.conf -Dtest_01_rpmsg_init_secondary_core_CONF_FILE=$(pwd)/middleware/multicore/rpmsg-lite/tests/prj-gcov-secondary.conf -DEXTRA_CFLAGS=-save-temps=obj -Dcore_id=cm33_core0
```

This command:
- Builds for the mimxrt700evk board
- Uses system build for multi-core compilation
- Specifies coverage configuration files for both primary and secondary cores
- Sets up additional flags needed for debug and coverage analysis

## 2. Running Tests with Coverage

### Using Ozone Debugger

When running with the Ozone debugger:

1. Load your project ELF file
2. Ensure semihosting is enabled (Options > J-Link > Semihosting)
3. Set the working directory to your build directory
4. Run the program
5. Coverage data files (.gcda) will be automatically written

### Using GDB/JLink from Command Line

To enable semihosting with `west debug -r jlink`:

```bash
west debug -r jlink
```

Then in the GDB console:

```
(gdb) monitor semihosting enable
(gdb) monitor semihosting IOClient 1
(gdb) monitor semihosting basedir /path/to/your/build/directory
(gdb) continue
```

## 3. Collecting and Processing Coverage Data

After running tests, you'll need to gather and process .gcda and .gcno files.

### Step 1: Create a Directory for Coverage Analysis

```bash
mkdir -p gcov
```

### Step 2: Collect All Coverage Files

This command gathers all .gcda and .gcno files, renaming them to avoid conflicts:

```bash
find . -path "./gcov" -prune -o \( -name "*.gcda" -o -name "*.gcno" \) -exec bash -c 'firstdir=$(echo "{}" | cut -d"/" -f2); filename=$(basename "{}"); cp "{}" "gcov/${firstdir}_${filename}"' \;
```

### Step 3: Process Files with gcov

```bash
cd gcov/
${ARMGCC_DIR}/bin/arm-none-eabi-gcov *.gcda
```

### Step 4: Generate Coverage Reports with lcov

```bash
# Capture baseline (zero coverage)
lcov --gcov-tool ${ARMGCC_DIR}/bin/arm-none-eabi-gcov --capture --initial --directory . --output-file coverage_base.info

# Capture actual coverage
lcov --gcov-tool ${ARMGCC_DIR}/bin/arm-none-eabi-gcov --capture --directory . --output-file coverage_test.info

# Combine the data
lcov --gcov-tool ${ARMGCC_DIR}/bin/arm-none-eabi-gcov --add-tracefile coverage_base.info --add-tracefile coverage_test.info --output-file coverage_total.info
```

### Step 5: Filter Results to Focus on Relevant Code

```bash
# Filter to only include files from our target component
lcov --extract coverage_total.info "*/middleware/multicore/rpmsg-lite/lib/*" --output-file coverage_filtered.info
```

### Step 6: Generate HTML Report

```bash
genhtml coverage_filtered.info --output-directory coverage_report
```

## 4. Complete One-Line Command

For convenience, here's a complete command that performs all these steps:

```bash
mkdir -p gcov && \
find . -path "./gcov" -prune -o \( -name "*.gcda" -o -name "*.gcno" \) -exec bash -c 'firstdir=$(echo "{}" | cut -d"/" -f2); filename=$(basename "{}"); cp "{}" "gcov/${firstdir}_${filename}"' \; && \
cd gcov/ && \
${ARMGCC_DIR}/bin/arm-none-eabi-gcov *.gcda && \
lcov --gcov-tool ${ARMGCC_DIR}/bin/arm-none-eabi-gcov --capture --initial --directory . --output-file coverage_base.info && \
lcov --gcov-tool ${ARMGCC_DIR}/bin/arm-none-eabi-gcov --capture --directory . --output-file coverage_test.info && \
lcov --gcov-tool ${ARMGCC_DIR}/bin/arm-none-eabi-gcov --add-tracefile coverage_base.info --add-tracefile coverage_test.info --output-file coverage_combined.info && \
lcov --extract coverage_combined.info "*/middleware/multicore/rpmsg-lite/lib/*" --output-file coverage_filtered.info && \
genhtml coverage_filtered.info --output-directory coverage_report
```

## 5. Troubleshooting

### Zero Coverage Data

If you're getting 0% coverage despite tests running:

1. Verify that semihosting is working by checking for "SysWrite" operations in debugger console
2. Ensure tests are actually executing the code paths you're measuring
3. Check if the coverage data files (.gcda) are being written to the expected location
4. Manually add a call to `gcov_dump_data()` at critical points in your code
5. Look for permission issues in your build directory

### Unexpected Files in Coverage Reports

Your coverage report may include unexpected files like system headers. Filter these out using:

```bash
lcov --remove coverage_combined.info "*/arch/arm/*" "*/drivers/*" "*/usr/local/*" --output-file coverage_filtered.info
```

### Multi-Core Issues

For multi-core systems:
- Each core needs its own coverage setup
- Use different output directories or file prefixes for each core's coverage data
- Process and merge the coverage data after collecting from all cores

## 6. Benefits of Code Coverage

Implementing code coverage analysis for embedded systems provides:

1. Verification that tests are exercising key code paths
2. Identification of dead code
3. Discovery of untested error handling paths
4. Metrics for test quality assessment
5. Documentation for certification requirements

By following this guide, you can integrate code coverage into your embedded development workflow and significantly improve test quality.
