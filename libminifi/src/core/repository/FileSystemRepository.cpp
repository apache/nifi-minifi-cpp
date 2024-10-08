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

#include "core/repository/FileSystemRepository.h"
#include <string>
#include <filesystem>
#include "io/FileStream.h"
#include "utils/file/FileUtils.h"
#include "core/ForwardingContentSession.h"

namespace org::apache::nifi::minifi::core::repository {

bool FileSystemRepository::initialize(const std::shared_ptr<Configure>& configuration) {
  if (std::string directory_str; configuration->get(Configure::nifi_dbcontent_repository_directory_default, directory_str) && !directory_str.empty()) {
    directory_ = directory_str;
  } else {
    directory_ = configuration->getHome().string();
  }
  utils::file::create_dir(directory_);
  return true;
}

std::shared_ptr<io::BaseStream> FileSystemRepository::write(const ResourceClaim& claim, bool append) {
  return std::make_shared<io::FileStream>(claim.getContentFullPath(), append);
}

bool FileSystemRepository::exists(const ResourceClaim& streamId) {
  const std::ifstream file(streamId.getContentFullPath());
  return file.good();
}

std::shared_ptr<io::BaseStream> FileSystemRepository::read(const ResourceClaim& claim) {
  return std::make_shared<io::FileStream>(claim.getContentFullPath(), 0, false);
}

bool FileSystemRepository::removeKey(const std::string& content_path) {
  logger_->log_debug("Deleting resource {}", content_path);
  std::error_code ec;
  const auto result = std::filesystem::exists(content_path, ec);
  if (ec) {
    logger_->log_error("Deleting {} from content repository failed with the following error: {}", content_path, ec.message());
    return false;
  }
  if (!result) {
    logger_->log_debug("Content path {} does not exist, no need to delete it", content_path);
    return true;
  }
  ec.clear();
  if (!std::filesystem::remove(content_path, ec)) {
    logger_->log_error("Deleting {} from content repository failed with the following error: {}", content_path, ec.message());
    return false;
  }
  return true;
}

std::shared_ptr<ContentSession> FileSystemRepository::createSession() {
  return std::make_shared<ForwardingContentSession>(sharedFromThis<ContentRepository>());
}

void FileSystemRepository::clearOrphans() {
  utils::file::list_dir(directory_, [&] (auto& /*dir*/, auto& filename) {
    auto path = directory_ +  "/" + filename.string();
    bool is_orphan = false;
    {
      std::lock_guard lock(count_map_mutex_);
      auto it = count_map_.find(path);
      is_orphan = it == count_map_.end() || it->second == 0;
    }
    if (is_orphan) {
      logger_->log_debug("Deleting orphan resource {}", path);
      if (std::error_code ec; !std::filesystem::remove(path, ec)) {
        {
          std::lock_guard<std::mutex> lock(purge_list_mutex_);
          purge_list_.push_back(path);
        }
        logger_->log_error("Deleting {} from content repository failed with the following error: {}", path, ec.message());
      }
    }
    return true;
  }, logger_, false);
}

size_t FileSystemRepository::size(const ResourceClaim& claim) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(claim.getContentFullPath(), ec);
  if (ec)
    return 0;
  return size;
}

uint64_t FileSystemRepository::getRepositoryEntryCount() const {
  auto dir_it = std::filesystem::recursive_directory_iterator(directory_, std::filesystem::directory_options::skip_permission_denied);
  return std::count_if(
    begin(dir_it),
    end(dir_it),
    [](auto& entry) { return entry.is_regular_file(); });
}

std::shared_ptr<core::ContentRepository> createFileSystemRepository() {
  return std::make_shared<FileSystemRepository>();
}

}  // namespace org::apache::nifi::minifi::core::repository
