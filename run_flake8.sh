#!/bin/bash

set -euo pipefail

directory=${1:-.}
flake8 --exclude thirdparty,build --ignore E501,E111,E114,W504 --per-file-ignores="steps.py:F811" "${directory}"
