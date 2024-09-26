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
#pragma once

#include <optional>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include "core/StateManager.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "aws/core/utils/DateTime.h"
#include "utils/file/FileUtils.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::aws::s3 {

struct MultipartUploadState {
  MultipartUploadState() = default;
  MultipartUploadState(std::string upload_id, uint64_t part_size, uint64_t full_size, const Aws::Utils::DateTime& upload_time)
    : upload_id(upload_id),
      part_size(part_size),
      full_size(full_size),
      upload_time_ms_since_epoch(upload_time.Millis()) {}
  std::string upload_id;
  size_t uploaded_parts{};
  uint64_t uploaded_size{};
  uint64_t part_size{};
  uint64_t full_size{};
  int64_t upload_time_ms_since_epoch;
  std::vector<std::string> uploaded_etags;

  bool operator==(const MultipartUploadState&) const = default;
};

class MultipartUploadStateStorage {
 public:
  explicit MultipartUploadStateStorage(gsl::not_null<core::StateManager*> state_manager)
    : state_manager_(state_manager) {
  }

  void storeState(const std::string& bucket, const std::string& key, const MultipartUploadState& state);
  std::optional<MultipartUploadState> getState(const std::string& bucket, const std::string& key) const;
  void removeState(const std::string& bucket, const std::string& key);
  void removeAgedStates(std::chrono::milliseconds multipart_upload_max_age_threshold);
  std::filesystem::path getStateFilePath() const;

 private:
  static void removeKey(const std::string& state_key, std::unordered_map<std::string, std::string>& state_map);

  mutable std::mutex state_manager_mutex_;
  gsl::not_null<core::StateManager*> state_manager_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<MultipartUploadStateStorage>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::aws::s3
