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
#include "LmdbFlowFileRepository.h"
#include "core/Resource.h"
#include "minifi-cpp/FlowFileRecord.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::lmdb {

namespace {
bool getRepositoryCheckHealth(const Configure& configure) {
  std::string check_health_str;
  configure.get(Configure::nifi_flow_file_repository_check_health, check_health_str);
  return utils::string::toBool(check_health_str).value_or(true);
}
}  // namespace

bool LmdbFlowFileRepository::initialize(const std::shared_ptr<Configure> &configure) {
  std::string value;

  if (configure->get(Configure::nifi_flowfile_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  }
  check_flowfile_content_size_ = getRepositoryCheckHealth(*configure);
  logger_->log_debug("NiFi LMDB FlowFile Repository Directory {}", directory_);

  // Reserve virtual address space for the DB file (max size it can grow to)
  const auto max_db_size = configure->get(Configure::nifi_flowfile_repository_lmdb_max_db_size) | utils::andThen([](auto max_db_size_str) -> std::optional<uint64_t> {
    if (max_db_size_str.empty()) { return std::nullopt; }
    return parsing::parseDataSize(max_db_size_str) | utils::orThrow(fmt::format("{} was set to invalid value: '{}'", Configure::nifi_flowfile_repository_lmdb_max_db_size, max_db_size_str));
  }) | utils::orElse([] {
    return std::make_optional<uint64_t>(MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE);
  });

  if (!max_db_size) {
    logger_->log_error("Invalid max DB size configuration for LMDB FlowFile Repository");
    return false;
  }

  logger_->log_info("Using LMDB FlowFile Repository directory '{}'", directory_);
  return lmdb_wrapper_.initialize(directory_, *max_db_size);
}

bool LmdbFlowFileRepository::Delete(const std::string& key) {
  keys_to_delete_.enqueue({.key = key});
  return true;
}

bool LmdbFlowFileRepository::Delete(const std::shared_ptr<core::CoreComponent>& item) {
  if (auto ff = std::dynamic_pointer_cast<core::FlowFile>(item)) {
    keys_to_delete_.enqueue({.key = item->getUUIDStr(), .content = ff->getResourceClaim()});
  } else {
    keys_to_delete_.enqueue({.key = item->getUUIDStr()});
  }
  return true;
}

bool LmdbFlowFileRepository::Put(const std::string& key, const uint8_t* buf, size_t bufLen) {
  return lmdb_wrapper_.putValue(key, std::string(reinterpret_cast<const char*>(buf), bufLen));
}

bool LmdbFlowFileRepository::MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) {
  return lmdb_wrapper_.putValues(data);
}

bool LmdbFlowFileRepository::Get(const std::string& key, std::string& value) {
  auto result = lmdb_wrapper_.getValue(key);
  if (result) {
    value = std::move(*result);
    return true;
  }
  return false;
}

uint64_t LmdbFlowFileRepository::getRepositorySize() const {
  const auto stat = lmdb_wrapper_.getDbStat();
  return stat.ms_psize * (stat.ms_branch_pages + stat.ms_leaf_pages + stat.ms_overflow_pages);
}

uint64_t LmdbFlowFileRepository::getRepositoryEntryCount() const {
  return lmdb_wrapper_.getDbStat().ms_entries;
}

void LmdbFlowFileRepository::flush() {
  std::list<ExpiredFlowFileInfo> flow_files;

  while (keys_to_delete_.size_approx() > 0) {
    ExpiredFlowFileInfo info;
    if (keys_to_delete_.try_dequeue(info)) {
      flow_files.push_back(std::move(info));
    }
  }

  deserializeFlowFilesWithNoContentClaim(flow_files);

  std::vector<std::string> flow_file_keys;
  for (auto& ff : flow_files) {
    flow_file_keys.push_back(ff.key);
    logger_->log_debug("Issuing batch delete, including {}, Content path {}", ff.key, ff.content ? ff.content->getContentFullPath() : "null");
  }

  if (!lmdb_wrapper_.removeKeys(flow_file_keys)) {
    for (auto&& ff : flow_files) {
      keys_to_delete_.enqueue(std::move(ff));
    }
    return;  // Stop here - don't delete from content repo while we have records in FF repo
  }

  if (content_repo_) {
    for (auto& ff : flow_files) {
      if (ff.content) {
        ff.content->decreaseFlowFileRecordOwnedCount();
      }
    }
  }
}

void LmdbFlowFileRepository::deserializeFlowFilesWithNoContentClaim(std::list<ExpiredFlowFileInfo>& flow_files) {
  std::vector<std::string> keys;
  std::vector<std::list<ExpiredFlowFileInfo>::iterator> key_positions;
  for (auto it = flow_files.begin(); it != flow_files.end(); ++it) {
    if (!it->content) {
      keys.push_back(it->key);
      key_positions.push_back(it);
    }
  }
  if (keys.empty()) {
    return;
  }
  std::vector<std::optional<std::string>> values;
  values.reserve(keys.size());
  for (const auto& key : keys) {
    values.push_back(lmdb_wrapper_.getValue(key));
  }

  gsl_Expects(keys.size() == values.size());

  for (size_t i = 0; i < keys.size(); ++i) {
    if (!values[i]) {
      logger_->log_error("Failed to read key from LMDB: {}! DB is most probably in an inconsistent state!", keys[i].data());
      flow_files.erase(key_positions.at(i));
      continue;
    }

    utils::Identifier container_id;
    auto flow_file = FlowFileRecord::DeSerialize(std::as_bytes(std::span(*values[i])), content_repo_, container_id);
    if (flow_file) {
      gsl_Expects(flow_file->getUUIDStr() == key_positions.at(i)->key);
      key_positions.at(i)->content = flow_file->getResourceClaim();
    } else {
      logger_->log_error("Could not deserialize flow file {}", key_positions.at(i)->key);
    }
  }
}

void LmdbFlowFileRepository::run() {
  while (isRunning()) {
    std::this_thread::sleep_for(purge_period_);
    flush();
  }
  flush();
}

bool LmdbFlowFileRepository::contentSizeIsAmpleForFlowFile(const core::FlowFile& flow_file_record, const std::shared_ptr<ResourceClaim>& resource_claim) const {
  const auto stream_size = resource_claim ? content_repo_->size(*resource_claim) : 0;
  const auto required_size = flow_file_record.getOffset() + flow_file_record.getSize();
  return stream_size >= required_size;
}

core::Connectable* LmdbFlowFileRepository::getContainer(const std::string& container_id) {
  auto container = containers_.find(container_id);
  if (container != containers_.end())
    return container->second;
  // for backward compatibility
  container = connection_map_.find(container_id);
  if (container != connection_map_.end())
    return container->second;
  return nullptr;
}

void LmdbFlowFileRepository::initialize_repository() {
  gsl_Expects(content_repo_);
  logger_->log_info("Reading existing flow files from database");

  lmdb_wrapper_.forEach([this](const MDB_val& key, const MDB_val& value) {
    utils::Identifier container_id;
    const std::string key_str = std::string(static_cast<char*>(key.mv_data), key.mv_size);
    const std::string data = std::string(static_cast<char*>(value.mv_data), value.mv_size);
    auto eventRead = FlowFileRecord::DeSerialize(std::as_bytes(std::span(data.data(), data.size())), content_repo_, container_id);
    if (!eventRead) {
      keys_to_delete_.enqueue({.key = key_str});
      return;
    }
    auto claim = eventRead->getResourceClaim();
    if (claim) {
      claim->increaseFlowFileRecordOwnedCount();
    }
    const auto container = getContainer(container_id.to_string());
    if (!container) {
      logger_->log_warn("Could not find connection for {}, path {}", container_id.to_string(), eventRead->getContentFullPath());
      keys_to_delete_.enqueue({.key = key_str, .content = eventRead->getResourceClaim()});
      return;
    }
    if (check_flowfile_content_size_ && !contentSizeIsAmpleForFlowFile(*eventRead, claim)) {
      logger_->log_warn("Content is missing or too small for flowfile {}", eventRead->getContentFullPath());
      keys_to_delete_.enqueue({.key = key_str, .content = eventRead->getResourceClaim()});
      return;
    }

    logger_->log_debug("Found connection for {}, path {}", container_id.to_string(), eventRead->getContentFullPath());
    eventRead->setStoredToRepository(true);
    // we found the connection for the persistent flowFile
    // even if a processor immediately marks it for deletion, flush only happens after prune_stored_flowfiles
    container->restore(eventRead);
  });

  flush();
  content_repo_->clearOrphans();
}

void LmdbFlowFileRepository::loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) {
  content_repo_ = content_repo;
  initialize_repository();
}

REGISTER_RESOURCE_AS(LmdbFlowFileRepository, InternalResource, ("LmdbFlowFileRepository", "lmdbflowfilerepository"));

}  // namespace org::apache::nifi::minifi::extensions::lmdb
