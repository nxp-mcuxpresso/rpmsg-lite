#!/usr/bin/env python3
#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

import os
import shutil
import glob
from pathlib import Path
import platform

def main():
    # Directory for coverage data
    current_dir = Path.cwd()
    coverage_dir = current_dir / "coverage_data"
    coverage_dir.mkdir(exist_ok=True)

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

    # Loop counter for iteration numbering
    counter = 1

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
        input("Press Enter when ready to collect coverage data for {}...".format(test))

        # Create directory for this test's coverage data
        test_coverage_dir = coverage_dir / test
        test_coverage_dir.mkdir(exist_ok=True)

        # Find and collect coverage data
        print(f"Collecting coverage data for {test}...")

        # Using glob for cross-platform file finding
        gcda_files = []
        gcno_files = []
        for pattern in ["**/*.gcda", "**/*.gcno"]:
            # Exclude files in the gcov directory
            for file_path in build_dir.glob(pattern):
                if "gcov" not in str(file_path):
                    if file_path.suffix == '.gcda':
                        gcda_files.append(file_path)
                    else:
                        gcno_files.append(file_path)

        # Process and copy found files
        for file_path in gcda_files + gcno_files:
            # Get the first directory component and filename
            try:
                # Handle both Windows and Linux paths
                # The original bash script takes the first directory after the build dir
                relative_path = file_path.relative_to(build_dir)
                parts = relative_path.parts

                # Get first directory if available, otherwise use "root"
                firstdir = parts[0] if len(parts) > 1 else "root"
                filename = file_path.name

                # Create destination filename with test name prefix
                dest_file = test_coverage_dir / f"{test}_{firstdir}_{filename}"

                # Copy the file
                shutil.copy2(file_path, dest_file)
                print(f"  Copied {file_path} → {dest_file}")

            except (ValueError, IndexError) as e:
                print(f"  Error processing {file_path}: {e}")

        print(f"Coverage data collected for {test}")
        print("----------------------------------------")

        # Increment counter for next iteration
        counter += 1

    print(f"All test coverage data collected in {coverage_dir}!")

if __name__ == "__main__":
    main()
