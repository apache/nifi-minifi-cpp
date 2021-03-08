#!/bin/bash

find . -type d \( -path "*thirdparty*" -o -path "*build*" \) -prune -false -o -type f -name "*.sh" | xargs shellcheck
