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

#include "core/Core.h"
#include <memory>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::shared_ptr<utils::IdGenerator> CoreComponent::id_generator_ = utils::IdGenerator::getIdGenerator();

// Set UUID
void CoreComponent::setUUID(uuid_t uuid) {
  uuid_copy(uuid_, uuid);
  char uuidStr[37];
  uuid_unparse_lower(uuid_, uuidStr);
  uuidStr_ = uuidStr;
}
// Get UUID
bool CoreComponent::getUUID(uuid_t uuid) {
  if (uuid) {
    uuid_copy(uuid, uuid_);
    return true;
  } else {
    return false;
  }
}

// Get UUID
unsigned const char *CoreComponent::getUUID() {
  return uuid_;
}

// Set Processor Name
void CoreComponent::setName(const std::string name) {
  name_ = name;
}
// Get Process Name
std::string CoreComponent::getName() {
  return name_;
}
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
