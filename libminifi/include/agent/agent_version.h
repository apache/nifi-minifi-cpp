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

#include <string>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class AgentBuild {
 public:
  static const char* const VERSION;
  static const char* const BUILD_IDENTIFIER;
  static const char* const BUILD_REV;
  static const char* const BUILD_DATE;
  static const char* const COMPILER;
  static const char* const COMPILER_VERSION;
  static const char* const COMPILER_FLAGS;
  static std::vector<std::string> getExtensions();
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_AGENT_AGENT_VERSION_H_
