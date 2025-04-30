#!/usr/bin/env python3
#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

import os
import subprocess
import platform
import sys
from pathlib import Path

def main():
    # Configuration
    BOARD = "mimxrt700evk"
    CONFIG = "flash_debug"
    CORE = "cm33_core0"

    # Get current working directory
    current_dir = Path.cwd()

    # Directory for coverage data
    COVERAGE_DIR = current_dir / "coverage_data"
    COVERAGE_DIR.mkdir(exist_ok=True)

    # Configuration files with gcov enabled
    PRIMARY_GCOV_CONF = current_dir / "middleware" / "multicore" / "rpmsg-lite" / "tests" / "prj-gcov-primary.conf"
    SECONDARY_GCOV_CONF = current_dir / "middleware" / "multicore" / "rpmsg-lite" / "tests" / "prj-gcov-secondary.conf"

    # Array of test directories
    TESTS = [
        "01_rpmsg_init",
        "01_rpmsg_init_rtos",
        "02_epts_channels",
        "02_epts_channels_rtos",
        "03_send_receive",
        "03_send_receive_rtos",
        "04_ping_pong",
        "04_ping_pong_rtos",
        "05_thread_safety_rtos"
    ]

    # Check if west is installed
    try:
        subprocess.run(["west", "--version"], stdout=subprocess.PIPE, check=True)
    except (subprocess.SubprocessError, FileNotFoundError):
        print("Error: 'west' tool not found. Please install it first.")
        print("Hint: pip install west")
        sys.exit(1)

    # Loop counter for iteration numbering
    counter = 1

    # Build each test
    for test in TESTS:
        print(f"==== Building {test} ====")

        # Create build directory for this test
        BUILD_DIR = Path(f"b_{counter}")

        # Build the test path
        test_path = Path("middleware") / "multicore" / "rpmsg-lite" / "tests" / test / "primary"

        # Get test name for secondary core config parameter
        test_name = test.replace("/", "_")

        # Using list form for better cross-platform compatibility
        cmd = [
            "west", "build",
            "-b", BOARD,
            str(test_path),
            "--sysbuild",
            "-p", "always",
            "--config", CONFIG,
            "--build-dir", str(BUILD_DIR),
            "--",
            f"-DCONF_FILE={str(PRIMARY_GCOV_CONF)}",
            f"-Dtest_{test_name}_secondary_core_CONF_FILE={str(SECONDARY_GCOV_CONF)}",
            "-DEXTRA_CFLAGS=-save-temps=obj",
            f"-Dcore_id={str(CORE)}"
        ]

        print(f"Running command: {' '.join(cmd)}")

        # Execute build command
        try:
            result = subprocess.run(cmd, check=True)
            print(f"Build finished for {test}")
        except subprocess.CalledProcessError as e:
            print(f"Error building {test}: {e}")
            sys.exit(1)

        print("----------------------------------------")

        # Increment counter for next iteration
        counter += 1

    print("All tests built successfully!")

if __name__ == "__main__":
    main()
