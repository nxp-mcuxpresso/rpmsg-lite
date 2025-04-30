#!/bin/bash
#
# Copyright 2025 NXP
#
# SPDX-License-Identifier: BSD-3-Clause

set -x

# Configuration
COVERAGE_DIR="coverage_data"
GCOV_CONF=$(pwd)/middleware/multicore/rpmsg-lite/tests/prj-gcov.conf
GCOV_TOOL="/usr/local/mcuxpressoide/ide/tools/bin/arm-none-eabi-gcov"
MERGED_DIR="$COVERAGE_DIR/merged"
REPORT_DIR="$COVERAGE_DIR/report"
TEST_TIMEOUT=120  # Timeout in seconds for each test

# Device settings - adjust based on your target board
DEVICE="MIMXRT1176_M7"
INTERFACE="SWD"
SPEED=4000

# Check if JLinkGDBServer is available
if ! command -v JLinkGDBServer &> /dev/null; then
    echo "ERROR: JLinkGDBServer not found in PATH"
    echo "Please ensure SEGGER J-Link software is installed and in your PATH"
    exit 1
fi

# Check for existing J-Link connections
pkill JLinkGDBServer >/dev/null 2>&1

# Array of test directories
TESTS=(
  "01_rpmsg_init"
)

# Create directories
mkdir -p $COVERAGE_DIR
mkdir -p $MERGED_DIR
mkdir -p $REPORT_DIR

# Create a GDB script with proper Python sleep function
cat > gdb_wait.py << EOF
import time
def gdb_sleep(seconds):
    time.sleep(seconds)
EOF

# Loop counter for iteration numbering
counter=1
total_tests=${#TESTS[@]}

# Build, run, and collect coverage for each test
for test in "${TESTS[@]}"; do
  echo "============================================"
  echo "==== Processing test $counter/$total_tests: $test ===="
  echo "============================================"

  # Create shortened build directory for this test with counter
  BUILD_DIR="b_${counter}"

  # Build the test
  echo "Building $test in directory $BUILD_DIR..."
  west build -b evkbmimxrt1170 middleware/multicore/rpmsg-lite/tests/$test/primary/ \
    -Dcore_id=cm7 --sysbuild -p always --config flexspi_nor_debug \
    --build-dir $BUILD_DIR -DCONF_FILE=$GCOV_CONF

  # Create directory for this test's coverage data
  TEST_COVERAGE_DIR="$COVERAGE_DIR/$test"
  mkdir -p $TEST_COVERAGE_DIR

  cd $BUILD_DIR
  if [ -f "primary/test_${test}_primary_core_cm7.elf" ]; then
    echo "Starting JLinkGDBServer..."
    # Start JLinkGDBServer with proper logging
    JLinkGDBServer -device $DEVICE -if $INTERFACE -speed $SPEED -endian little -localhostonly > jlink_log.txt 2>&1 &
    JLINK_PID=$!

    # Give server time to start and verify it's running
    echo "Waiting for JLinkGDBServer to initialize..."
    sleep 5
    if ! ps -p $JLINK_PID > /dev/null; then
      echo "ERROR: JLinkGDBServer failed to start. Check jlink_log.txt for details."
      cat jlink_log.txt
      cd ..
      continue
    fi

    # Create a GDB command file with connection retry logic
    cat > gdb_commands.gdb << EOF
# Source Python script for sleep function
source ../gdb_wait.py

# Define function to try connection with timeout
define connect_with_retry
  set \$retries = 5
  set \$connected = 0
  while (\$retries > 0 && \$connected == 0)
    echo Connecting to target, \$retries attempts left...\n

    # Try to connect
    target extended-remote localhost:2331

    # If we get here, connection succeeded
    set \$connected = 1

    # Wait a moment before proceeding
    python gdb_sleep(1)
  end
  if (\$connected == 0)
    echo Failed to connect to target\n
    quit 1
  end
end

# Try to connect with retries
connect_with_retry

# Setup target
monitor reset
load
monitor semihosting enable
monitor semihosting IOClient 1
monitor semihosting basedir $(pwd)

# Run the program
continue

# Use Python for sleeping
python gdb_sleep(${TEST_TIMEOUT})

# Get coverage data
interrupt
call gcov_dump_data()

# Exit GDB
quit
EOF

    # Run GDB with our command file
    echo "Running test with GDB..."
    /usr/local/mcuxpressoide/ide/tools/bin/arm-none-eabi-gdb -x gdb_commands.gdb "primary/test_${test}_primary_core_cm7.elf"
    GDB_RESULT=$?

    # Kill the GDB server
    echo "Stopping JLinkGDBServer..."
    kill $JLINK_PID 2>/dev/null || pkill JLinkGDBServer

    # Check result
    if [ $GDB_RESULT -ne 0 ]; then
      echo "ERROR: GDB execution failed with code $GDB_RESULT"
      echo "Check jlink_log.txt for JLink issues:"
      cat jlink_log.txt
    else
      echo "GDB execution completed successfully"
    fi
  else
    echo "ERROR: ELF file not found: primary/test_${test}_primary_core_cm7.elf"
  fi
  cd ..

  # Find and collect coverage data
  echo "Collecting coverage data for test $counter: $test..."
  find $BUILD_DIR -path "$BUILD_DIR/gcov" -prune -o \( -name "*.gcda" -o -name "*.gcno" \) \
    -exec bash -c 'firstdir=$(echo "{}" | cut -d"/" -f2); filename=$(basename "{}"); cp "{}" "'$TEST_COVERAGE_DIR'/${firstdir}_${filename}"' \;

  echo "Coverage data collected for test $counter: $test"
  echo "----------------------------------------"

  # Increment counter for next iteration
  ((counter++))
done

# Clean up
rm -f gdb_wait.py

# Process coverage data
echo "Merging coverage data from all tests..."
find $COVERAGE_DIR -path "$MERGED_DIR" -prune -o -path "$REPORT_DIR" -prune -o \( -name "*.gcda" -o -name "*.gcno" \) \
  -exec cp {} $MERGED_DIR \;

# Process with gcov
cd $MERGED_DIR
$GCOV_TOOL *.gcda

# Generate coverage info
lcov --gcov-tool $GCOV_TOOL --capture --initial --directory . --output-file coverage_base.info
lcov --gcov-tool $GCOV_TOOL --capture --directory . --output-file coverage_test.info
lcov --gcov-tool $GCOV_TOOL --add-tracefile coverage_base.info --add-tracefile coverage_test.info --output-file coverage_combined.info

# Filter to focus on rpmsg-lite code
lcov --extract coverage_combined.info "*/middleware/multicore/rpmsg-lite/lib/*" --output-file coverage_filtered.info

# Generate HTML report
genhtml coverage_filtered.info --output-directory $REPORT_DIR

echo "============================================"
echo "Complete coverage report generated in $REPORT_DIR"
echo "Open $REPORT_DIR/index.html in your browser to view the report"
echo "============================================"
