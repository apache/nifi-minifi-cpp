#!/bin/bash

# A script to compare the contents of an RPM package against an expected file list.

# --- Usage ---
# ./check_rpm_contents.sh <path/to/your.rpm> <path/to/expected_list.txt>

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <RPM_FILE> <EXPECTED_LIST_FILE>"
    exit 1
fi

RPM_FILE="$1"
EXPECTED_LIST_FILE="$2"

if [ ! -f "$RPM_FILE" ]; then
    echo "Error: RPM file not found at '$RPM_FILE'"
    exit 1
fi

if [ ! -f "$EXPECTED_LIST_FILE" ]; then
    echo "Error: Expected list file not found at '$EXPECTED_LIST_FILE'"
    exit 1
fi

# Create temporary files for sorted lists
# mktemp ensures we have a unique and secure temporary file
ACTUAL_SORTED=$(mktemp)
EXPECTED_SORTED=$(mktemp)

trap 'rm -f "$ACTUAL_SORTED" "$EXPECTED_SORTED"' EXIT

echo "--- Analyzing RPM package: $RPM_FILE ---"

rpm -qlp "$RPM_FILE" | sort > "$ACTUAL_SORTED"
sort "$EXPECTED_LIST_FILE" > "$EXPECTED_SORTED"

DIFFERENCES=$(diff "$ACTUAL_SORTED" "$EXPECTED_SORTED")

if ! DIFFERENCES=$(diff "$ACTUAL_SORTED" "$EXPECTED_SORTED"); then
    echo "❌ FAILURE: The RPM contents do not match the expected list."
    echo ""
    echo "--- Differences ---"
    echo "< Lines only in the RPM"
    echo "> Lines only in your expected list"
    echo "-------------------"
    echo "$DIFFERENCES"
    exit 1
else
    echo "✅ SUCCESS: The RPM contents match the expected list."
    exit 0
fi