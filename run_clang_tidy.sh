#!/bin/bash

set -uo pipefail

FILE=$1

EXCLUDED_EXTENSIONS=("pdh" "windows-event-log")
EXCLUDED_DIRECTORY=("nanofi")

for excluded_extension in "${EXCLUDED_EXTENSIONS[@]}"; do
  if [[ "${FILE}" =~ extensions/${excluded_extension}/ ]]; then
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

clang-tidy-14 -warnings-as-errors=* -quiet -p build "$FILE"
