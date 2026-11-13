#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

set -e

version=$1
src_dir=$2
out_dir=$3
compiler=$4
compiler_version=$5
flags=$6
buildident=$7

date=$(date +%s)

if [ -d "${src_dir}"/.git ]; then
  buildrev=$(git log -1 --pretty=format:"%H")
else
  buildrev="Unknown"
fi

cat >"$out_dir/agent_version.cpp" <<EOF
// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "minifi-cpp/agent/agent_version.h"

namespace org::apache::nifi::minifi {

const char* const AgentBuild::VERSION = "$version";
const char* const AgentBuild::BUILD_IDENTIFIER = "$buildident";
const char* const AgentBuild::BUILD_REV = "$buildrev";
const char* const AgentBuild::BUILD_DATE = "$date";
const char* const AgentBuild::COMPILER = "$compiler";
const char* const AgentBuild::COMPILER_VERSION = "$compiler_version";
const char* const AgentBuild::COMPILER_FLAGS = R"($flags)";

}  // namespace org::apache::nifi::minifi

EOF
