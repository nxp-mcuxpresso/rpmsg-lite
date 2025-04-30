#!/usr/bin/env python3
#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

import os
import sys
import shutil
import subprocess
import glob
from pathlib import Path

def find_gcov_tool():
    """Find the arm-none-eabi-gcov tool using ARMGCC_DIR environment variable or default paths."""

    # First check ARMGCC_DIR environment variable
    armgcc_dir = os.environ.get('ARMGCC_DIR')
    if armgcc_dir:
        # On Windows, the executable has .exe extension
        if sys.platform == "win32":
            gcov_path = Path(armgcc_dir) / "bin" / "arm-none-eabi-gcov.exe"
        else:
            gcov_path = Path(armgcc_dir) / "bin" / "arm-none-eabi-gcov"

        if gcov_path.exists():
            print(f"Found gcov tool using ARMGCC_DIR: {gcov_path}")
            return gcov_path

    # Default locations to check if ARMGCC_DIR is not set or tool not found there
    possible_paths = [
        # Linux/macOS
        Path("/usr/local/mcuxpressoide/ide/tools/bin/arm-none-eabi-gcov"),
        # Windows typical locations
        Path("C:/Program Files/MCUXpressoIDE/ide/tools/bin/arm-none-eabi-gcov.exe"),
        Path("C:/nxp/MCUXpressoIDE/ide/tools/bin/arm-none-eabi-gcov.exe"),
    ]

    # Check default locations
    for path in possible_paths:
        if path.exists():
            return path

    # Check in PATH
    try:
        # For Windows
        if sys.platform == "win32":
            result = subprocess.run(["where", "arm-none-eabi-gcov"],
                                   capture_output=True, text=True, check=False)
            if result.returncode == 0:
                return Path(result.stdout.splitlines()[0].strip())
        # For Linux/macOS
        else:
            result = subprocess.run(["which", "arm-none-eabi-gcov"],
                                   capture_output=True, text=True, check=False)
            if result.returncode == 0:
                return Path(result.stdout.strip())
    except Exception:
        pass

    return None

def main():
    # Directory for coverage data
    current_dir = Path.cwd()
    coverage_dir = current_dir / "coverage_data"
    report_dir = coverage_dir / "report"
    coverage_dir.mkdir(exist_ok=True)
    report_dir.mkdir(exist_ok=True)

    # Array of test directories
    tests = [
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

    # Check if gcovr is installed
    try:
        subprocess.run(["gcovr", "--version"], stdout=subprocess.PIPE, check=True)
    except (subprocess.SubprocessError, FileNotFoundError):
        print("Error: gcovr is not installed. Please install it with:")
        print("pip install gcovr")
        sys.exit(1)

    # Path to gcov tool
    gcov_tool = find_gcov_tool()
    if not gcov_tool:
        print("Error: Could not find arm-none-eabi-gcov in default locations or PATH")
        print("Please ensure the correct gcov tool is available and update the script if needed")
        sys.exit(1)

    print(f"Using gcov tool: {gcov_tool}")

    # Loop counter for iteration numbering
    counter = 1
    gcov_dirs = []

    for test in tests:
        print(f"==== Running {test} ====")

        build_dir = Path(f"b_{counter}")

        # Instructions for running the test
        print("Please follow these steps:")
        print(f"1. Connect to your board using Ozone or JLink")
        print(f"2. Enable semihosting and set the base directory to: {current_dir / build_dir}")
        print(f"3. Load and run the ELF file: {build_dir / 'primary' / f'test_{test}_primary_core_cm7.elf'}")
        print(f"4. Wait for the test to complete")
        print(f"5. Press Enter after the test is complete to continue")
        input(f"Press Enter when ready to collect coverage data for {test}...")

        # Create directory for this test's coverage data
        test_coverage_dir = coverage_dir / test
        test_coverage_dir.mkdir(exist_ok=True)

        # Find and collect coverage data
        print(f"Collecting coverage data for {test}...")

        # Using pathlib for cross-platform file finding
        gcda_files = []
        gcno_files = []
        for pattern in ["**/*.gcda", "**/*.gcno"]:
            for file_path in build_dir.glob(pattern):
                if "gcov" not in str(file_path):
                    if file_path.suffix == '.gcda':
                        gcda_files.append(file_path)
                    else:
                        gcno_files.append(file_path)

        # Process and copy found files
        for file_path in gcda_files + gcno_files:
            try:
                # Get the first directory component and filename
                relative_path = file_path.relative_to(build_dir)
                parts = relative_path.parts

                # Get first directory if available, otherwise use "root"
                firstdir = parts[0] if len(parts) > 1 else "root"
                filename = file_path.name

                # Create destination filename with test name prefix
                dest_file = test_coverage_dir / f"{test}_{firstdir}_{filename}"

                # Copy the file
                shutil.copy2(file_path, dest_file)
                print(f"  Copied {file_path.name}")

            except (ValueError, IndexError) as e:
                print(f"  Error processing {file_path}: {e}")

        print(f"Coverage data collected for {test}")

        # Add this test's coverage directory to our list of dirs to process
        gcov_dirs.append(test_coverage_dir)

        print("----------------------------------------")

        # Increment counter for next iteration
        counter += 1

    print(f"All test coverage data collected in {coverage_dir}!")

    # Generate coverage report using gcovr
    print("Generating coverage report with gcovr...")

    cmd = [
        "gcovr",
        f"--gcov-executable={gcov_tool.as_posix()}",
        f"--html-details={report_dir.as_posix()}/index.html",
        "--html-medium-threshold=75",
        "--html-high-threshold=90",
        "--filter=.*rpmsg-lite/lib/.*",
        "--merge-mode-functions=merge-use-line-min",
        "--gcov-ignore-parse-errors=negative_hits.warn_once_per_file",
        "--exclude-unreachable-branches",
    ]

    # Add coverage directories to the command with proper path handling
    for dir_path in gcov_dirs:
        cmd.append(dir_path.as_posix())

    try:
        subprocess.run(cmd, check=True)
        print("============================================")
        print(f"Coverage report generated in {report_dir}")
        print(f"Open {report_dir}/index.html in your browser to view the report")
        print("============================================")
    except subprocess.CalledProcessError as e:
        print(f"Error generating coverage report: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
