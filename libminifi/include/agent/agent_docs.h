/*** Licensed to the Apache Software Foundation (ASF) under one or more
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
#ifndef LIBMINIFI_INCLUDE_AGENT_AGENT_DOCS_H_
#define LIBMINIFI_INCLUDE_AGENT_AGENT_DOCS_H_

#include <stdlib.h>
#include <utils/StringUtils.h>

#include <map>
#include <string>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
class AgentDocs {
 private:
  static std::map<std::string, std::string> &getDescriptions();

 public:
  /**
   * Updates the internal map with the feature description
   * @param feature feature ( CS or processor ) whose description is being provided.
   * @param description provided description.
   * @return true if update occurred.
   */
  static bool putDescription(const std::string &feature, const std::string &description) {
    return getDescriptions().insert(std::make_pair(feature, description)).second;
  }

  /**
   * Gets the description for the provided feature.
   * @param feature feature whose description we will provide
   * @return true if found, false otherwise
   */
  static bool getDescription(const std::string &feature, std::string &value) {
    const std::map<std::string, std::string> &extensions = getDescriptions();
    auto iff = extensions.find(feature);
    if (iff != extensions.end()) {
      value = iff->second;
      return true;
    } else {
      return false;
    }
  }
};
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_AGENT_AGENT_DOCS_H_
