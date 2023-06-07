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

MultipartUploadStateStorage::MultipartUploadStateStorage(const std::string& state_directory, const std::string& state_id) {
  if (state_directory.empty()) {
    char format[] = "/var/tmp/nifi-minifi-cpp.s3-multipart-upload.XXXXXX";
    state_file_path_ = minifi::utils::file::FileUtils::create_temp_directory(format);
  } else {
    state_file_path_ = std::filesystem::path(state_directory) / std::string(state_id + "-s3-multipart-upload-state.properties");
    if (!std::filesystem::exists(state_file_path_)) {
      std::filesystem::create_directories(state_file_path_.parent_path());
      std::ofstream ofs(state_file_path_);
    } else {
      loadFile();
    }
  }
}

void MultipartUploadStateStorage::storeState(const std::string& bucket, const std::string& key, const MultipartUploadState& state) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  std::string state_key = bucket + "/" + key;
  state_[state_key + ".upload_id"] = state.upload_id;
  state_[state_key + ".upload_time"] = std::to_string(state.upload_time.Millis());
  state_[state_key + ".uploaded_parts"] = std::to_string(state.uploaded_parts);
  state_[state_key + ".uploaded_size"] = std::to_string(state.uploaded_size);
  state_[state_key + ".part_size"] = std::to_string(state.part_size);
  state_[state_key + ".full_size"] = std::to_string(state.full_size);
  state_[state_key + ".uploaded_etags"] = minifi::utils::StringUtils::join(";", state.uploaded_etags);
  commitChanges();
  logger_->log_debug("Updated multipart upload state with key %s", state_key);
}

std::optional<MultipartUploadState> MultipartUploadStateStorage::getState(const std::string& bucket, const std::string& key) const {
  std::string state_key = bucket + "/" + key;
  if (!state_.contains(state_key + ".upload_id")) {
    logger_->log_warn("Failed to get state: Multipart upload state was not found for key '%s'", state_key);
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(state_mutex_);
  MultipartUploadState state;
  state.upload_id = state_.at(state_key + ".upload_id");

  int64_t stored_upload_time = 0;
  core::Property::StringToInt(state_.at(state_key + ".upload_time"), stored_upload_time);
  state.upload_time = Aws::Utils::DateTime(stored_upload_time);

  core::Property::StringToInt(state_.at(state_key + ".uploaded_parts"), state.uploaded_parts);
  core::Property::StringToInt(state_.at(state_key + ".uploaded_size"), state.uploaded_size);
  core::Property::StringToInt(state_.at(state_key + ".part_size"), state.part_size);
  core::Property::StringToInt(state_.at(state_key + ".full_size"), state.full_size);
  state.uploaded_etags = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(state_.at(state_key + ".uploaded_etags"), ";");
  return state;
}

void MultipartUploadStateStorage::removeState(const std::string& bucket, const std::string& key) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  std::string state_key = bucket + "/" + key;
  if (!state_.contains(state_key + ".upload_id")) {
    logger_->log_warn("Multipart upload state was not found for key '%s'", state_key);
    return;
  }

  removeKey(state_key);
  commitChanges();
  logger_->log_debug("Removed multipart upload state with key %s", state_key);
}

void MultipartUploadStateStorage::removeAgedStates(std::chrono::milliseconds multipart_upload_max_age_threshold) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  auto age_off_time = Aws::Utils::DateTime::Now() - multipart_upload_max_age_threshold;

  std::vector<std::string> keys_to_remove;
  for (const auto& [property_key, value] : state_) {
    if (!minifi::utils::StringUtils::endsWith(property_key, ".upload_time")) {
      continue;
    }
    if (!state_.contains(property_key)) {
      logger_->log_error("Could not retrieve value for multipart upload cache key '%s'", property_key);
      continue;
    }
    int64_t stored_upload_time{};
    if (!core::Property::StringToInt(value, stored_upload_time)) {
      logger_->log_error("Multipart upload cache key '%s' has invalid value '%s'", property_key, value);
      continue;
    }
    auto upload_time = Aws::Utils::DateTime(stored_upload_time);
    if (upload_time < age_off_time) {
      auto state_key_and_property_name = minifi::utils::StringUtils::split(property_key, ".");
      if (state_key_and_property_name.size() < 2) {
        logger_->log_error("Invalid property '%s'", property_key);
        continue;
      }
      keys_to_remove.push_back(state_key_and_property_name[0]);
    }
  }
  for (const auto& key : keys_to_remove) {
    logger_->log_info("Removing local aged off multipart upload state with key '%s'", key);
    removeKey(key);
  }
  commitChanges();
}

void MultipartUploadStateStorage::removeKey(const std::string& state_key) {
  state_.erase(state_key + ".upload_id");
  state_.erase(state_key + ".upload_time");
  state_.erase(state_key + ".uploaded_parts");
  state_.erase(state_key + ".uploaded_size");
  state_.erase(state_key + ".part_size");
  state_.erase(state_key + ".full_size");
  state_.erase(state_key + ".uploaded_etags");
}

void MultipartUploadStateStorage::loadFile() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  state_.clear();
  std::ifstream ifs(state_file_path_);
  if (!ifs.is_open()) {
    logger_->log_error("Failed to open multipart upload state file '%s'", state_file_path_.string());
    return;
  }
  std::string line;
  while (std::getline(ifs, line)) {
    auto key_and_value = minifi::utils::StringUtils::split(line, "=");
    if (key_and_value.size() < 2) {
      continue;
    }
    state_[key_and_value[0]] = key_and_value[1];
  }
}

void MultipartUploadStateStorage::commitChanges() {
  std::ofstream ofs(state_file_path_);
  if (!ofs.is_open()) {
    logger_->log_error("Failed to open multipart upload state file '%s'", state_file_path_.string());
    return;
  }

  for (const auto& [key, value] : state_) {
    ofs << key << "=" << value << "\n";
  }
}

}  // namespace org::apache::nifi::minifi::aws::s3
