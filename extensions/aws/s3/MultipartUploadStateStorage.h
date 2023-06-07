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

#include "core/StateManager.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "aws/core/utils/DateTime.h"
#include "properties/Properties.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::aws::s3 {

struct MultipartUploadState {
  MultipartUploadState() = default;
  MultipartUploadState(std::string upload_id, uint64_t part_size, uint64_t full_size, const Aws::Utils::DateTime& upload_time)
    : upload_id(upload_id),
      part_size(part_size),
      full_size(full_size),
      upload_time(upload_time) {}
  std::string upload_id;
  size_t uploaded_parts{};
  uint64_t uploaded_size{};
  uint64_t part_size{};
  uint64_t full_size{};
  Aws::Utils::DateTime upload_time;
  std::vector<std::string> uploaded_etags;

  bool operator==(const MultipartUploadState&) const = default;
};

class MultipartUploadStateStorage {
 public:
  MultipartUploadStateStorage(const std::string& state_directory, const std::string& state_id);

  void storeState(const std::string& bucket, const std::string& key, const MultipartUploadState& state);
  std::optional<MultipartUploadState> getState(const std::string& bucket, const std::string& key) const;
  void removeState(const std::string& bucket, const std::string& key);
  void removeAgedStates(std::chrono::milliseconds multipart_upload_max_age_threshold);

 private:
  std::filesystem::path state_file_path_;
  minifi::Properties state_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<MultipartUploadStateStorage>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::aws::s3
