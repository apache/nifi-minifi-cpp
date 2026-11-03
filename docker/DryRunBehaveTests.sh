#!/bin/bash

# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

print_help()
{
  printf '%s\n' "Dry runs the behave feature directories found in the provided directories"
  printf '%s\n' "to catch environment.py and step module loading errors, which are silently skipped by behavex"
  printf 'Usage: %s [--tags_to_run=<arg>] [--tags_to_exclude=<arg>] [-h|--help] <search_dir> ...\n' "$0"
  printf '\t%s\n' "<search_dir>: directory to search for feature files recursively"
  printf '\t%s\n' "--tags_to_run: optional comma-separated list of tags to include, only the feature directories having features with any of these tags are checked (default: all)"
  printf '\t%s\n' "--tags_to_exclude: optional comma-separated list of tags that should be skipped (default: none)"
  printf '\t%s\n' "-h, --help: Prints help"
}

tags_to_run=
tags_to_exclude=
search_dirs=()
for arg in "$@"; do
  case "${arg}" in
    --tags_to_run=*)
      tags_to_run="${arg##--tags_to_run=}"
      ;;
    --tags_to_exclude=*)
      tags_to_exclude="${arg##--tags_to_exclude=}"
      ;;
    -h|--help)
      print_help
      exit 0
      ;;
    *)
      search_dirs+=("${arg}")
      ;;
  esac
done

if [ "${#search_dirs[@]}" -eq 0 ]; then
  print_help >&2
  exit 1
fi

root_dir="$( cd "${0%/*}/.." && pwd )"

# Create virtual environment for testing
if [[ ! -d ./behave_venv ]]; then
  echo "Creating virtual environment in ./behave_venv" 1>&2
  virtualenv --python=python3 ./behave_venv
fi

echo "Activating virtual environment..." 1>&2
# shellcheck disable=SC1091
. ./behave_venv/bin/activate
pip install -e "${root_dir}/behave_framework"

TAG_OPTS=()
if [ -n "${tags_to_run}" ]; then
  TAG_OPTS+=(-t "${tags_to_run}")
fi
IFS=',' read -ra excluded_tags <<< "${tags_to_exclude}"
for tag in "${excluded_tags[@]}"; do
  TAG_OPTS+=(-t "~${tag}")
done

FEATURE_FILES=()
while IFS= read -r -d '' file; do
  FEATURE_FILES+=("${file}")
done < <(find "${search_dirs[@]}" -name .venv -prune -o -type f -name '*.feature' -print0)

if [ "${#FEATURE_FILES[@]}" -eq 0 ]; then
  echo "No feature files found in: ${search_dirs[*]}" >&2
  exit 1
fi

# Scenario Outlines without example rows are silently dropped by behave
python3 -I - "${FEATURE_FILES[@]}" <<'EOF'
import sys
from behave.model import ScenarioOutline
from behave.parser import parse_file

empty_outlines = []
for feature_file in sys.argv[1:]:
    for scenario in parse_file(feature_file).walk_scenarios(with_outlines=True):
        if isinstance(scenario, ScenarioOutline) and not any(examples.table and examples.table.rows for examples in scenario.examples):
            empty_outlines.append(f"{feature_file}:{scenario.line}: {scenario.name}")

if empty_outlines:
    print("Scenario Outlines without examples found, use Scenario instead:", file=sys.stderr)
    print("\n".join(f"  {outline}" for outline in empty_outlines), file=sys.stderr)
    sys.exit(1)
EOF

FEATURE_DIRS=()
while IFS= read -r dir; do
  FEATURE_DIRS+=("${dir}")
done < <(
  if [ -n "${tags_to_run}" ]; then
    grep -lE "@(${tags_to_run//,/|})([[:space:]]|$)" "${FEATURE_FILES[@]}"
  else
    printf '%s\n' "${FEATURE_FILES[@]}"
  fi | sed 's|/[^/]*$||' | sort -u)

failed_dirs=()
for dir in "${FEATURE_DIRS[@]}"; do
  echo "Dry run of ${dir}" 1>&2
  behave --dry-run --no-summary --format null "${TAG_OPTS[@]}" "${dir}" || failed_dirs+=("${dir}")
done

if [ "${#failed_dirs[@]}" -gt 0 ]; then
  echo "Behave dry run failed, the following feature directories could not be loaded:" >&2
  printf '  %s\n' "${failed_dirs[@]}" >&2
  exit 1
fi

echo "Behave dry run succeeded for ${#FEATURE_DIRS[@]} feature directories" 1>&2
