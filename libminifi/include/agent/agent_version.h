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
#ifndef LIBMINIFI_INCLUDE_AGENT_AGENT_VERSION_H_
#define LIBMINIFI_INCLUDE_AGENT_AGENT_VERSION_H_

#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class AgentBuild {
 public:
  static constexpr const char* VERSION = "0.7.0";
  static constexpr const char* BUILD_IDENTIFIER = "RhqnAg2TrbAq2GRPspiPl6DG";
  static constexpr const char* BUILD_REV = "13403fb5a920c451081fcbc3050249d0ba0a462b";
  static constexpr const char* BUILD_DATE = "1595341791";
  static constexpr const char* COMPILER = "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++";
  static constexpr const char* COMPILER_VERSION = "11.0.3.11030032";
  static constexpr const char* COMPILER_FLAGS = " -std=c++11 -DOPENSSL_SUPPORT";
  static std::vector<std::string> getExtensions() {
    static std::vector<std::string> extensions;
    if (extensions.empty()) {
      extensions.push_back("minifi-standard-processors");
      extensions.push_back("minifi-http-curl");
      extensions.push_back("minifi-expression-language-extensions");
      extensions.push_back("minifi-civet-extensions");
      extensions.push_back("minifi-rocksdb-repos");
      extensions.push_back("minifi-archive-extensions");
      extensions.push_back("minifi-script-extensions");
      extensions.push_back("minifi-sftp");
      extensions.push_back("minifi-system");
    }
    return extensions;
  }
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_AGENT_AGENT_VERSION_H_
