/**
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

#include "agent/agent_docs.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

std::map<std::string, Components> AgentDocs::class_mappings_;

bool AgentDocs::getDescription(const std::string &feature, std::string &value) {
  for (const auto& [module_name, module_classes] : class_mappings_) {
    for (const auto& class_descriptions : {module_classes.processors_, module_classes.controller_services_, module_classes.other_components_}) {
      for (const auto& class_description : class_descriptions) {
        if (class_description.full_name_ == feature || class_description.short_name_ == feature) {
          value = class_description.description_;
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
