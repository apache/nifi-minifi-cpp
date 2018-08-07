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

version=$1
src_dir=$2
out_dir=$3
compiler=$4
compiler_version=$5
flags=$6
extensions=$7
buildident=$8

date=`date +%s`

if [ -d ${src_dir}/.git ]; then
  buildrev=`git log -1 --pretty=format:"%H"`
  hostname=`hostname`
else
  buildrev="Unknown"
fi

IFS=';' read -r -a extensions_array <<< "$extensions"

extension_list="${extension_list} } "

cat >"$out_dir/agent_version.h" <<EOF
#ifndef AGENT_BUILD_H
#define AGENT_BUILD_H

#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class AgentBuild {
 public:
  static constexpr const char* VERSION = "$version";
  static constexpr const char* BUILD_IDENTIFIER = "$buildident";
  static constexpr const char* BUILD_REV = "$buildrev";
  static constexpr const char* BUILD_DATE = "$date";
  static constexpr const char* COMPILER = "$compiler";
  static constexpr const char* COMPILER_VERSION = "$compiler_version";
  static constexpr const char* COMPILER_FLAGS = "$flags";
  static std::vector<std::string> getExtensions() {
  	static std::vector<std::string> extensions;
  	if (extensions.empty()){
EOF

for EXTENSION in "${extensions_array[@]}"
do
cat <<EOF >> "$out_dir/agent_version.h"
      extensions.push_back("${EXTENSION}");
EOF
done

cat <<EOF >> "$out_dir/agent_version.h"
      extensions.push_back("minifi-system");
    }
  	return extensions;
  }
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* AGENT_BUILD_H */
EOF
