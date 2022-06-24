#!/bin/bash

set -uo pipefail

exit_code=0
FILES=$1

EXCLUDED_EXTENSIONS=("pdh" "windows-event-log" "tensorflow")

for changed_file in ${FILES}; do
  for extension in "${EXCLUDED_EXTENSIONS[@]}"; do
    if [[ "${changed_file}" =~ extensions/${extension}/ ]]; then
      continue 2
    fi
  done
  if [[ "${changed_file}" == *.cpp ]] && [[ -f "${changed_file}" ]]; then
    clang-tidy-14 -warnings-as-errors=* -quiet -p build "${changed_file}"
    exit_code=$(( $? | exit_code ))
  fi;
done

exit $exit_code
