// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "core/BulletinStore.h"

#include <ranges>
#include "core/logging/LoggerBase.h"

namespace org::apache::nifi::minifi::core {

BulletinStore::BulletinStore(const Configure &configure) {
  auto max_bulletin_count_str = configure.get(Configuration::nifi_c2_flow_info_processor_bulletin_limit);
  if (!max_bulletin_count_str) {
    logger_->log_debug("Bulletin limit not set, using default value of {}", DEFAULT_BULLETIN_COUNT);
    max_bulletin_count_ = DEFAULT_BULLETIN_COUNT;
    return;
  }
  try {
    max_bulletin_count_ = std::stoul(*max_bulletin_count_str);
  } catch(const std::exception&) {
    logger_->log_warn("Invalid value for bulletin limit, using default value of {}", DEFAULT_BULLETIN_COUNT);
    max_bulletin_count_ = DEFAULT_BULLETIN_COUNT;
  }
}

void BulletinStore::addProcessorBulletin(const core::Processor& processor, core::logging::LOG_LEVEL log_level, const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  Bulletin bulletin;
  bulletin.id = id_counter++;
  bulletin.timestamp = std::chrono::system_clock::now();
  bulletin.level = core::logging::mapLogLevelToString(log_level);
  bulletin.category = "Log Message";
  bulletin.message = message;
  bulletin.group_id = processor.getProcessGroupUUIDStr();
  bulletin.group_name = processor.getProcessGroupName();
  bulletin.group_path = processor.getProcessGroupPath();
  bulletin.source_id = processor.getUUIDStr();
  bulletin.source_name = processor.getName();
  if (bulletins_.size() >= max_bulletin_count_) {
    bulletins_.pop_front();
  }
  bulletins_.push_back(std::move(bulletin));
}

std::deque<Bulletin> BulletinStore::getBulletins(std::optional<std::chrono::system_clock::duration> time_interval_to_include) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!time_interval_to_include) {
    return bulletins_;
  }

  const auto timestamp_cutoff = std::chrono::system_clock::now() - *time_interval_to_include;
  auto it = std::lower_bound(bulletins_.begin(), bulletins_.end(), timestamp_cutoff,
    [](const auto& bulletin, const auto& timestamp) {
      return bulletin.timestamp < timestamp;
    });
  if (it != bulletins_.end()) {
    return {it, bulletins_.end()};
  }

  return {};
}

std::vector<Bulletin> BulletinStore::getBulletinsForProcessor(const std::string& processor_uuid) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return bulletins_
      | std::ranges::views::filter([&](const auto& elem) { return elem.source_id == processor_uuid; })
      | ranges::to<std::vector>();
}

size_t BulletinStore::getMaxBulletinCount() const {
  return max_bulletin_count_;
}

}  // namespace org::apache::nifi::minifi::core
