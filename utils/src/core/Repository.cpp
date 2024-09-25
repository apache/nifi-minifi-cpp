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
#include "core/Repository.h"

namespace org::apache::nifi::minifi::core {

bool RepositoryImpl::Delete(std::vector<std::shared_ptr<core::SerializableComponent>> &storedValues) {
  bool found = true;
  for (const auto& storedValue : storedValues) {
    found &= Delete(storedValue->getName());
  }
  return found;
}

bool RepositoryImpl::storeElement(const std::shared_ptr<core::SerializableComponent>& element) {
  if (!element) {
    return false;
  }

  org::apache::nifi::minifi::io::BufferStream stream;

  element->serialize(stream);

  if (!Put(element->getUUIDStr(), reinterpret_cast<const uint8_t*>(stream.getBuffer().data()), stream.size())) {
    logger_->log_error("NiFi Provenance Store event {} size {} fail", element->getUUIDStr(), stream.size());
    return false;
  }
  return true;
}

}  // namespace org::apache::nifi::minifi::core
