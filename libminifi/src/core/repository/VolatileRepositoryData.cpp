// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "core/repository/VolatileRepositoryData.h"

#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/utils/gsl.h"

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
