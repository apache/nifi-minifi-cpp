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

namespace org::apache::nifi::minifi::core {

CoreComponentImpl::CoreComponentImpl(std::string_view name, const utils::Identifier& uuid, const std::shared_ptr<utils::IdGenerator>& idGenerator)
    : name_(name) {
  if (uuid.isNil()) {
    uuid_ = idGenerator->generate();
  } else {
    uuid_ = uuid;
  }
}

void CoreComponentImpl::setUUID(const utils::Identifier& uuid) {
  uuid_ = uuid;
}

utils::Identifier CoreComponentImpl::getUUID() const {
  return uuid_;
}

void CoreComponentImpl::setName(std::string name) {
  name_ = std::move(name);
}

std::string CoreComponentImpl::getName() const {
  return name_;
}

} /* namespace org::apache::nifi::minifi::core */
