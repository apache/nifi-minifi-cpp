#!/bin/bash

set -uo pipefail

FILE=$1

EXCLUDED_DIRECTORY=("extensions/pdh" "extensions/windows-event-log" "extensions/smb")
EXCLUDED_FILES=("WindowsCertStoreLocationTests.cpp")

for excluded_file in "${EXCLUDED_FILES[@]}"; do
  if [[ "${FILE}" =~ ${excluded_file} ]]; then
    exit 0
  fi
done

for excluded_directory in "${EXCLUDED_DIRECTORY[@]}"; do
  if [[ "${FILE}" =~ ${excluded_directory}/ ]]; then
    exit 0
  fi
done

if ! [[ "${FILE}" == *.cpp ]]; then
  exit 0
fi

if ! [[ -f "${FILE}" ]]; then
  exit 0
fi

clang-tidy-20 -warnings-as-errors=* -quiet -p build "$FILE"
