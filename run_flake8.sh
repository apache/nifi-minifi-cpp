#!/bin/bash

set -euo pipefail

directory=${1:-.}
flake8 --exclude thirdparty,build --builtins log,REL_SUCCESS,REL_FAILURE,raw_input --ignore E501,W503 --per-file-ignores="steps.py:F811" "${directory}"
