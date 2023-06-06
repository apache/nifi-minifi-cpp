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
#include "MultipartUploadStateStorage.h"

#include <unordered_map>

#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::aws::s3 {

MultipartUploadStateStorage::MultipartUploadStateStorage(const std::string& state_directory, std::string state_id)
    : state_id_(std::move(state_id)),
      state_(state_id_ + "-s3-multipart-upload-state.properties") {
  if (state_directory.empty()) {
    char format[] = "/var/tmp/nifi-minifi-cpp.s3-multipart-upload.XXXXXX";
    state_file_path_ = minifi::utils::file::FileUtils::create_temp_directory(format);
  } else {
    state_file_path_ = std::filesystem::path(state_directory) / std::string(state_id_ + "-s3-multipart-upload-state.properties");
    if (!std::filesystem::exists(state_file_path_)) {
      std::filesystem::create_directories(state_file_path_.parent_path());
      std::ofstream ofs(state_file_path_);
      ofs.close();
    } else {
      state_.loadFile(state_file_path_);
    }
  }
}

void MultipartUploadStateStorage::storeState(const std::string& bucket, const std::string& key, const MultipartUploadState& state) {
  state_.loadFile(state_file_path_);
  std::string state_key = bucket + "/" + key;
  state_.set(state_key + ".upload_id", state.upload_id);
  state_.set(state_key + ".upload_time", std::to_string(state.upload_time.Millis()));
  state_.set(state_key + ".uploaded_parts", std::to_string(state.uploaded_parts));
  state_.set(state_key + ".uploaded_size", std::to_string(state.uploaded_size));
  state_.set(state_key + ".part_size", std::to_string(state.part_size));
  state_.set(state_key + ".full_size", std::to_string(state.full_size));
  state_.set(state_key + ".uploaded_etags", minifi::utils::StringUtils::join(";", state.uploaded_etags));
  state_.commitChanges();
  logger_->log_debug("Updated multipart upload state with key %s", state_key);
}

std::optional<MultipartUploadState> MultipartUploadStateStorage::getState(const std::string& bucket, const std::string& key) const {
  std::string state_key = bucket + "/" + key;
  if (!state_.has(state_key + ".upload_id")) {
    logger_->log_warn("Failed to get state: Multipart upload state was not found for key '%s'", state_key);
    return std::nullopt;
  }

  MultipartUploadState state;
  state_.getString(state_key + ".upload_id", state.upload_id);

  int64_t stored_upload_time = 0;
  std::string value;
  if (state_.getString(state_key + ".upload_time", value)) {
    core::Property::StringToInt(value, stored_upload_time);
    state.upload_time = Aws::Utils::DateTime(stored_upload_time);
  }

  if (state_.getString(state_key + ".uploaded_parts", value)) {
    core::Property::StringToInt(value, state.uploaded_parts);
  }

  if (state_.getString(state_key + ".uploaded_size", value)) {
    core::Property::StringToInt(value, state.uploaded_size);
  }

  if (state_.getString(state_key + ".part_size", value)) {
    core::Property::StringToInt(value, state.part_size);
  }

  if (state_.getString(state_key + ".full_size", value)) {
    core::Property::StringToInt(value, state.full_size);
  }

  if (state_.getString(state_key + ".uploaded_etags", value)) {
    state.uploaded_etags = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(value, ";");
  }
  return state;
}

void MultipartUploadStateStorage::removeState(const std::string& bucket, const std::string& key) {
  state_.loadFile(state_file_path_);
  std::string state_key = bucket + "/" + key;
  if (!state_.has(state_key + ".upload_id")) {
    logger_->log_warn("Failed to remove state: Multipart upload state was not found for key '%s'", state_key);
    return;
  }

  state_.remove(state_key + ".upload_id");
  state_.remove(state_key + ".upload_time");
  state_.remove(state_key + ".uploaded_parts");
  state_.remove(state_key + ".uploaded_size");
  state_.remove(state_key + ".part_size");
  state_.remove(state_key + ".full_size");
  state_.remove(state_key + ".uploaded_etags");
  state_.commitChanges();
  logger_->log_debug("Removed multipart upload state with key %s", state_key);
}

}  // namespace org::apache::nifi::minifi::aws::s3
