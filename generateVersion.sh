#!/usr/bin/env bash
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
/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
