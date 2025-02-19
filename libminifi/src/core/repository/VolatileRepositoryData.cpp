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
#include "core/repository/VolatileRepositoryData.h"

#include "core/Property.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core::repository {

VolatileRepositoryData::VolatileRepositoryData(uint32_t max_count, size_t max_size)
  : current_size(0),
    current_entry_count(0),
    max_count(max_count),
    max_size(max_size) {
}

VolatileRepositoryData::~VolatileRepositoryData() {
  clear();
}

void VolatileRepositoryData::clear() {
  for (auto ent : value_vector) {
    delete ent;  // NOLINT(cppcoreguidelines-owning-memory)
  }
  value_vector.clear();
}

void VolatileRepositoryData::initialize(const std::shared_ptr<Configure> &configure, const std::string& repo_name) {
  if (configure != nullptr) {
    std::stringstream strstream;
    strstream << Configure::nifi_volatile_repository_options << repo_name << "." << VOLATILE_REPO_MAX_COUNT;
    std::string value;
    if (configure->get(strstream.str(), value)) {
      if (const auto max_cnt = parsing::parseIntegral<uint32_t>(value)) {
        max_count = *max_cnt;
      }
    }

    strstream.str("");
    strstream.clear();
    strstream << Configure::nifi_volatile_repository_options << repo_name << "." << VOLATILE_REPO_MAX_BYTES;
    if (configure->get(strstream.str(), value)) {
      if (const auto max_bytes = parsing::parseIntegral<int64_t>(value)) {
        if (*max_bytes <= 0) {
          max_size = std::numeric_limits<uint32_t>::max();
        } else {
          max_size = gsl::narrow<size_t>(*max_bytes);
        }
      }
    }
  }

  value_vector.reserve(max_count);
  for (uint32_t i = 0; i < max_count; i++) {
    value_vector.emplace_back(new AtomicEntry<std::string>(&current_size, &max_size));  // NOLINT(cppcoreguidelines-owning-memory)
  }
}

}  // namespace org::apache::nifi::minifi::core::repository
