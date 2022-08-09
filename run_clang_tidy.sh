#!/bin/bash

set -uo pipefail

exit_code=0
FILES=$1

EXCLUDED_EXTENSIONS=("pdh" "windows-event-log" "tensorflow")
EXCLUDED_DIRECTORY=("nanofi")

for changed_file in ${FILES}; do
  for excluded_extension in "${EXCLUDED_EXTENSIONS[@]}"; do
    if [[ "${changed_file}" =~ extensions/${excluded_extension}/ ]]; then
      continue 2
    fi
  done
  for excluded_directory in "${EXCLUDED_DIRECTORY[@]}"; do
    if [[ "${changed_file}" =~ ${excluded_directory}/ ]]; then
      continue 2
    fi
  done
  if [[ "${changed_file}" == *.cpp ]] && [[ -f "${changed_file}" ]]; then
    clang-tidy-14 -warnings-as-errors=* -quiet -p build "${changed_file}"
    exit_code=$(( $? | exit_code ))
  fi;
done

exit $exit_code
