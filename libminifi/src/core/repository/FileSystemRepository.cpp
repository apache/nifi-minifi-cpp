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
#include <memory>
#include <string>
#include "io/FileStream.h"
#include "utils/file/FileUtils.h"
#include "core/ForwardingContentSession.h"

namespace org::apache::nifi::minifi::core::repository {

bool FileSystemRepository::initialize(const std::shared_ptr<minifi::Configure>& configuration) {
  std::string directory_str;
  if (configuration->get(Configure::nifi_dbcontent_repository_directory_default, directory_str) && !directory_str.empty()) {
    directory_ = directory_str;
  } else {
    directory_ = configuration->getHome().string();
  }
  utils::file::create_dir(directory_);
  return true;
}

std::shared_ptr<io::BaseStream> FileSystemRepository::write(const minifi::ResourceClaim& claim, bool append) {
  return std::make_shared<io::FileStream>(claim.getContentFullPath(), append);
}

bool FileSystemRepository::exists(const minifi::ResourceClaim& streamId) {
  std::ifstream file(streamId.getContentFullPath());
  return file.good();
}

std::shared_ptr<io::BaseStream> FileSystemRepository::read(const minifi::ResourceClaim& claim) {
  return std::make_shared<io::FileStream>(claim.getContentFullPath(), 0, false);
}

bool FileSystemRepository::remove(const minifi::ResourceClaim& claim) {
  logger_->log_debug("Deleting resource %s", claim.getContentFullPath());
  std::remove(claim.getContentFullPath().c_str());
  return true;
}

std::shared_ptr<ContentSession> FileSystemRepository::createSession() {
  return std::make_shared<ForwardingContentSession>(sharedFromThis());
}

void FileSystemRepository::clearOrphans() {
  std::lock_guard<std::mutex> lock(count_map_mutex_);
  utils::file::list_dir(directory_, [&] (auto& /*dir*/, auto& filename) {
    auto path = directory_ +  "/" + filename.string();
    auto it = count_map_.find(path);
    if (it == count_map_.end() || it->second == 0) {
      logger_->log_debug("Deleting orphan resource %s", path);
      std::remove(path.c_str());
    }
    return true;
  }, logger_, false);
}

}  // namespace org::apache::nifi::minifi::core::repository
