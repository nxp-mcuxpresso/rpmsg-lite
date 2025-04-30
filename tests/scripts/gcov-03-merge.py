#!/usr/bin/env python3
#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

import os
import sys
import shutil
import subprocess
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
            return str(gcov_path)

    # If not found with ARMGCC_DIR, check system PATH
    try:
        # For Windows
        if sys.platform == "win32":
            result = subprocess.run(["where", "arm-none-eabi-gcov"],
                                   capture_output=True, text=True, check=False)
            if result.returncode == 0:
                return result.stdout.splitlines()[0].strip()
        # For Linux/macOS
        else:
            result = subprocess.run(["which", "arm-none-eabi-gcov"],
                                   capture_output=True, text=True, check=False)
            if result.returncode == 0:
                return result.stdout.strip()
    except Exception:
        pass

    return None

def check_lcov_tools():
    """Check if lcov and genhtml are installed."""
    try:
        subprocess.run(["lcov", "--version"], stdout=subprocess.PIPE, check=True)
        subprocess.run(["genhtml", "--version"], stdout=subprocess.PIPE, check=True)
        return True
    except (subprocess.SubprocessError, FileNotFoundError):
        print("Error: lcov or genhtml is not installed.")
        if sys.platform == "linux":
            print("On Ubuntu/Debian, install with: sudo apt-get install lcov")
        elif sys.platform == "darwin":  # macOS
            print("On macOS, install with: brew install lcov")
        elif sys.platform == "win32":
            print("On Windows, you can install lcov through MSYS2 or WSL")
        return False

def main():
    # Check for required tools
    if not check_lcov_tools():
        sys.exit(1)

    # Find gcov tool
    gcov_tool = find_gcov_tool()
    if not gcov_tool:
        print("Error: Could not find arm-none-eabi-gcov")
        print("Please set ARMGCC_DIR environment variable correctly")
        sys.exit(1)

    # Root coverage directory
    current_dir = Path.cwd()
    coverage_dir = current_dir / "coverage_data"
    merged_dir = coverage_dir / "merged"
    report_dir = coverage_dir / "report"

    # Create directories
    merged_dir.mkdir(exist_ok=True)
    report_dir.mkdir(exist_ok=True)

    print(f"Using gcov tool: {gcov_tool}")
    print(f"Merging coverage data from {coverage_dir}")

    # Copy all .gcno and .gcda files to the merged directory
    gcno_files = list(coverage_dir.glob("**/*.gcno"))
    gcda_files = list(coverage_dir.glob("**/*.gcda"))

    # Make sure we don't copy from merged or report directory
    exclude_dirs = [str(merged_dir), str(report_dir)]

    for file_path in gcno_files + gcda_files:
        # Skip files in merged or report directory
        if not any(excl in str(file_path) for excl in exclude_dirs):
            dest_file = merged_dir / file_path.name
            shutil.copy2(file_path, dest_file)
            print(f"Copied {file_path.name}")

    # Process with gcov - need to change to merged directory
    original_dir = os.getcwd()
    os.chdir(merged_dir)

    # Run gcov on all .gcda files
    try:
        gcda_files_in_merged = list(Path('.').glob("*.gcda"))
        if gcda_files_in_merged:
            gcda_files_str = " ".join([file.name for file in gcda_files_in_merged])
            cmd = f"{gcov_tool} {gcda_files_str}"
            subprocess.run(cmd, shell=True, check=True)
        else:
            print("Warning: No .gcda files found in merged directory")
    except subprocess.CalledProcessError as e:
        print(f"Error running gcov: {e}")

    # Generate coverage info with lcov
    print("\nGenerating coverage info with lcov...")
    try:
        # Base coverage (from .gcno files)
        subprocess.run([
            "lcov",
            f"--gcov-tool={gcov_tool}",
            "--capture",
            "--initial",
            "--directory", ".",
            "--output-file", "coverage_base.info"
        ], check=True)

        # Test coverage (from .gcda files)
        subprocess.run([
            "lcov",
            f"--gcov-tool={gcov_tool}",
            "--capture",
            "--directory", ".",
            "--output-file", "coverage_test.info"
        ], check=True)

        # Combine coverage data
        subprocess.run([
            "lcov",
            f"--gcov-tool={gcov_tool}",
            "--add-tracefile", "coverage_base.info",
            "--add-tracefile", "coverage_test.info",
            "--output-file", "coverage_combined.info"
        ], check=True)

        # Filter to focus on rpmsg-lite code
        subprocess.run([
            "lcov",
            "--extract", "coverage_combined.info",
            "*/middleware/multicore/rpmsg-lite/lib/*",
            "--output-file", "coverage_filtered.info"
        ], check=True)

        # Generate HTML report
        subprocess.run([
            "genhtml",
            "coverage_filtered.info",
            "--output-directory", str(report_dir)
        ], check=True)

        print(f"\nComplete coverage report generated in {report_dir}")
        print(f"Open {report_dir}/index.html in your browser to view the report")

    except subprocess.CalledProcessError as e:
        print(f"Error generating coverage report: {e}")

    # Return to original directory
    os.chdir(original_dir)

if __name__ == "__main__":
    main()
