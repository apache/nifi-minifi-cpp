#!/bin/bash

set -euo pipefail

directory=${1:-.}
find "${directory}" -type d \( -path "*thirdparty*" -o -path "*build*" \) -prune -false -o -type f -name "*.sh" | xargs shellcheck --exclude=SC1090,SC1091
