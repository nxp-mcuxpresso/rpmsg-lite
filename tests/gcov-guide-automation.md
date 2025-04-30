# RPMsg-Lite Test Coverage Automation Guide

This guide explains how to use the West extension for automating code coverage analysis of the RPMsg-Lite component in the MCUX SDK.

## Overview

The West GCOV extension provides a unified command-line interface for all coverage tasks:

- Building tests with code coverage instrumentation
- Collecting coverage data from running tests
- Generating coverage reports using gcovr and/or lcov

## Prerequisites

- MCUX SDK with RPMsg-Lite tests
- Python 3.6 or newer
- West tool installed (`pip install west`)
- J-Link debugger hardware connected to your target device
- ARM GCC toolchain (set ARMGCC_DIR environment variable)
- Target hardware (e.g., mimxrt700evk)

For coverage reports:
- lcov and/or gcovr installed

## Quick Start

```bash
# Create a default configuration file
west gcov --create-config

# Edit the configuration file to match your environment
# nano gcov_config.yaml

# Run the full coverage process
west gcov -t all -c gcov_config.yaml --output-dir my_coverage
```

## Configuration

The West GCOV extension uses a YAML configuration file to control all aspects of the coverage process. You can create a default configuration file with:

```bash
west gcov --create-config
```

This creates a file named `gcov_config.yaml` with default settings. Edit this file to match your project:

```yaml
# Top-level output directory for all artifacts
output_dir: "gcov_output"

# Build configuration
build:
  board: "mimxrt700evk"
  build_config: "flash_debug"
  core: "cm33_core0"
  build_dir_template: "b_{counter}"
  test_path_template: "middleware/multicore/rpmsg-lite/tests/{test}/primary"
  test_paths: "middleware/multicore/rpmsg-lite/tests/"
  primary_gcov_conf: "middleware/multicore/rpmsg-lite/tests/prj-gcov-primary.conf"
  secondary_gcov_conf: "middleware/multicore/rpmsg-lite/tests/prj-gcov-secondary.conf"

# Coverage configuration
coverage:
  lcov_filter_pattern: "*/middleware/multicore/rpmsg-lite/lib/*"
  gcovr_filter_pattern: ".*rpmsg-lite/lib/.*"
  report_tool: "both"  # Can be "lcov", "gcovr", or "both"
  data_dir: "coverage_data"

# Tests to run
tests:
  - "01_rpmsg_init"
  - "01_rpmsg_init_rtos"
  - "02_epts_channels"
  - "02_epts_channels_rtos"
  - "03_send_receive"
  - "03_send_receive_rtos"
  - "04_ping_pong"
  - "04_ping_pong_rtos"
  - "05_thread_safety_rtos"
```

## Step-by-Step Usage

### Step 1: Build Tests with Coverage Instrumentation

Build all tests with code coverage instrumentation:

```bash
west gcov -t build -c gcov_config.yaml
```

### Step 2: Collect Coverage Data

Run the collection task to execute tests and collect coverage data:

```bash
west gcov -t collect -c gcov_config.yaml
```

During this step, you'll be guided through the process of running each test on your target hardware:

1. For each test, the tool will display instructions
2. Connect to your board using Ozone or JLink
3. Enable semihosting and set the base directory as instructed
4. Load and run the ELF file on your target
5. After the test completes, press Enter to continue to the next test

### Step 3: Generate Coverage Reports

Generate coverage reports using either lcov, gcovr, or both:

```bash
west gcov -t report -c gcov_config.yaml
```

This creates HTML reports in the coverage output directory.

### Combined Approach

Run all tasks (build, collect, report) in a single command:

```bash
west gcov -t all -c gcov_config.yaml
```

## Customizing Execution

You can override configuration options with command-line arguments:

```bash
# Specify a different board
west gcov -t all -c gcov_config.yaml --board mimxrt1170evk

# Run only specific tests
west gcov -t all -c gcov_config.yaml --tests 01_rpmsg_init 04_ping_pong

# Use a different output directory
west gcov -t all -c gcov_config.yaml --output-dir custom_coverage_output

# Generate only gcovr reports
west gcov -t all -c gcov_config.yaml --report-tool gcovr
```

## Viewing the Results

After running the report task, open the HTML reports in your browser:

```bash
# For lcov report
firefox gcov_output/coverage_data/report/lcov/index.html

# For gcovr report
firefox gcov_output/coverage_data/report/gcovr/index.html
```

## Troubleshooting

If you encounter issues:

1. **Environment Setup**:
   - Ensure ARMGCC_DIR is set correctly: `echo $ARMGCC_DIR` (Linux/macOS) or `echo %ARMGCC_DIR%` (Windows)
   - Make sure Python 3.6+ is installed: `python3 --version` or `python --version`
   - Verify that West is installed: `west --version`

2. **JLink Connection Problems**: 
   - Ensure your hardware is properly connected and powered
   - Check the J-Link logs for specific errors

3. **No Coverage Data Found**:
   - Verify that semihosting is enabled in your debugger
   - Check that the base directory is set correctly
   - Make sure tests execute completely before collecting data

4. **Coverage Report Issues**:
   - Ensure lcov and/or gcovr are installed
   - Check if coverage data files (.gcda and .gcno) exist

## Future Enhancements

Future versions may include:
- Automated test execution via GDB
- Direct integration with CI/CD pipelines
- Additional filtering options for reports

## Current Limitations

- Currently tested only with multicore applications (rpmsg-lite, mcmgr)
- Manual test execution steps are required (debugger interaction)
