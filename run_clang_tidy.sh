#!/bin/bash

set -uo pipefail

exit_code=0
FILES=$1

for changed_file in ${FILES}; do
  if [[ "${changed_file}" == *.cpp ]]; then
    clang-tidy-14 -warnings-as-errors=* -quiet -p build "${changed_file}"
    exit_code=$(( $? | exit_code ))
  fi;
done

exit $exit_code
