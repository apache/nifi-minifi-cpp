#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

set -euo pipefail

directory=${1:-.}
find "${directory}" -type d \( -path "*thirdparty*" -o -path "*build*" -o -path "*venv*" \) -prune -false -o -type f -name "*.sh" | xargs shellcheck --exclude=SC1090,SC1091
