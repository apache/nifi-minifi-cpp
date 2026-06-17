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

#include "LmdbContentRepository.h"

#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "LmdbStream.h"
#include "core/Resource.h"
#include "lmdb.h"
#include "minifi-cpp/Exception.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/Locations.h"

namespace org::apache::nifi::minifi::extensions::lmdb {

LmdbContentRepository::Session::Session(std::shared_ptr<ContentRepository> repository) : BufferedContentSession(std::move(repository)) {}

void LmdbContentRepository::Session::commit() {
  auto lmdb_content_repository = std::dynamic_pointer_cast<LmdbContentRepository>(repository_);
  if (!lmdb_content_repository) { throw Exception(REPOSITORY_EXCEPTION, "Session's repository is not an LmdbContentRepository"); }

  const auto writeResource = [&lmdb_content_repository](const std::shared_ptr<ResourceClaim>& resource_claim, const std::shared_ptr<io::BaseStream>& stream, bool is_append) {
    auto outStream = lmdb_content_repository->write(*resource_claim, is_append);
    if (outStream == nullptr) { throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for write: " + resource_claim->getContentFullPath()); }
    const auto size = stream->size();
    if (outStream->write(stream->getBuffer()) != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to write " + std::string(is_append ? "appended" : "new") + " resource: " + resource_claim->getContentFullPath());
    }
    auto lmdb_out_stream = std::dynamic_pointer_cast<LmdbStream>(outStream);
    if (lmdb_out_stream == nullptr) { throw Exception(REPOSITORY_EXCEPTION, "Couldn't cast output stream to LmdbStream for commit: " + resource_claim->getContentFullPath()); }
    if (!lmdb_out_stream->commit()) { throw Exception(REPOSITORY_EXCEPTION, "Failed to commit " + std::string(is_append ? "appended" : "new") + " resource: " + resource_claim->getContentFullPath()); }
  };

  for (const auto& resource : managed_resources_) {
    writeResource(resource.first, resource.second, false);
  }

  for (const auto& resource : append_state_) {
    writeResource(resource.first, resource.second.stream, true);
  }

  managed_resources_.clear();
  append_state_.clear();
}

bool LmdbContentRepository::initialize(const std::shared_ptr<minifi::Configure>& configuration) {
  // Reserve virtual address space for the DB file (max size it can grow to)
  const auto max_db_size = configuration->get(Configure::nifi_content_repository_lmdb_max_db_size) | utils::andThen([](auto max_db_size_str) -> std::optional<uint64_t> {
    if (max_db_size_str.empty()) { return std::nullopt; }
    return parsing::parseDataSize(max_db_size_str) | utils::orThrow(fmt::format("{} was set to invalid value: '{}'", Configure::nifi_content_repository_lmdb_max_db_size, max_db_size_str));
  }) | utils::orElse([] {
    // Default to 10 GB if the property is not set
    return std::make_optional<uint64_t>(10ULL * 1024 * 1024 * 1024);
  });

  if (!max_db_size) {
    logger_->log_error("Invalid max DB size configuration for LMDB Content Repository");
    return false;
  }

  const auto working_dir = utils::getMinifiDir();

  std::string value;
  if (configuration->get(Configure::nifi_dbcontent_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  } else {
    directory_ = (working_dir / "lmdbcontentrepository").string();
  }
  logger_->log_info("Using LMDB Content Repository directory '{}'", directory_);

  return lmdb_wrapper_.initialize(directory_, *max_db_size);
}

std::shared_ptr<core::ContentSession> LmdbContentRepository::createSession() {
  return std::make_shared<Session>(sharedFromThis<core::ContentRepository>());
}

std::shared_ptr<io::BaseStream> LmdbContentRepository::write(const minifi::ResourceClaim& claim, bool) {
  return std::make_shared<LmdbStream>(claim.getContentFullPath(), lmdb_wrapper_, true);
}

std::shared_ptr<io::BaseStream> LmdbContentRepository::read(const minifi::ResourceClaim& claim) {
  return std::make_shared<LmdbStream>(claim.getContentFullPath(), lmdb_wrapper_, false);
}

bool LmdbContentRepository::exists(const minifi::ResourceClaim& streamId) {
  const auto path = streamId.getContentFullPath();
  return lmdb_wrapper_.exists(path);
}

bool LmdbContentRepository::removeKey(const std::string& content_path) {
  return lmdb_wrapper_.removeKey(content_path);
}

void LmdbContentRepository::clearOrphans() {
  std::vector<std::string> keys_to_be_deleted;

  lmdb_wrapper_.forEach([this, &keys_to_be_deleted](const MDB_val& key, const MDB_val&) {
    std::string key_string = std::string(static_cast<char*>(key.mv_data), key.mv_size);

    std::lock_guard<std::mutex> lock(count_map_mutex_);
    auto claim_it = count_map_.find(key_string);
    if (claim_it == count_map_.end() || claim_it->second == 0) {
      logger_->log_debug("Deleting orphan resource {}", key_string);
      keys_to_be_deleted.push_back(key_string);
    }
  });

  std::vector<std::string> failed_deletions;
  for (const auto& key : keys_to_be_deleted) {
    auto delete_result = removeKey(key);
    if (!delete_result) {
      logger_->log_warn("Failed to delete orphan resource {} from LMDB database", key);
      failed_deletions.push_back(key);
    }
  }

  std::lock_guard<std::mutex> lock(purge_list_mutex_);
  purge_list_.insert(purge_list_.end(), std::make_move_iterator(failed_deletions.begin()), std::make_move_iterator(failed_deletions.end()));
}

uint64_t LmdbContentRepository::getRepositorySize() const {
  const auto stat = lmdb_wrapper_.getDbStat();
  return stat.ms_psize * (stat.ms_branch_pages + stat.ms_leaf_pages + stat.ms_overflow_pages);
}

uint64_t LmdbContentRepository::getRepositoryEntryCount() const {
  return lmdb_wrapper_.getDbStat().ms_entries;
}

REGISTER_RESOURCE_AS(LmdbContentRepository, InternalResource, ("LmdbContentRepository", "lmdbcontentrepository"));

}  // namespace org::apache::nifi::minifi::extensions::lmdb
