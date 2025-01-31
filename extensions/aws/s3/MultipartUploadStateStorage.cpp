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

#include "utils/StringUtils.h"
#include "core/Property.h"

namespace org::apache::nifi::minifi::aws::s3 {

void MultipartUploadStateStorage::storeState(const std::string& bucket, const std::string& key, const MultipartUploadState& state) {
  std::unordered_map<std::string, std::string> stored_state;
  std::lock_guard<std::mutex> lock(state_manager_mutex_);
  state_manager_->get(stored_state);
  std::string state_key = bucket + "/" + key;
  stored_state[state_key + ".upload_id"] = state.upload_id;
  stored_state[state_key + ".upload_time"] = std::to_string(state.upload_time_ms_since_epoch);
  stored_state[state_key + ".uploaded_parts"] = std::to_string(state.uploaded_parts);
  stored_state[state_key + ".uploaded_size"] = std::to_string(state.uploaded_size);
  stored_state[state_key + ".part_size"] = std::to_string(state.part_size);
  stored_state[state_key + ".full_size"] = std::to_string(state.full_size);
  stored_state[state_key + ".uploaded_etags"] = minifi::utils::string::join(";", state.uploaded_etags);
  state_manager_->set(stored_state);
  state_manager_->commit();
  state_manager_->persist();
}

std::optional<MultipartUploadState> MultipartUploadStateStorage::getState(const std::string& bucket, const std::string& key) const {
  std::unordered_map<std::string, std::string> state_map;
  {
    std::lock_guard<std::mutex> lock(state_manager_mutex_);
    if (!state_manager_->get(state_map)) {
      logger_->log_warn("No previous multipart upload state was associated with this processor.");
      return std::nullopt;
    }
  }

  std::string state_key = bucket + "/" + key;
  if (!state_map.contains(state_key + ".upload_id")) {
    logger_->log_warn("Multipart upload state was not found for key '{}'", state_key);
    return std::nullopt;
  }

  MultipartUploadState state;
  state.upload_id = state_map[state_key + ".upload_id"];

  core::Property::StringToInt(state_map[state_key + ".upload_time"], state.upload_time_ms_since_epoch);
  core::Property::StringToInt(state_map[state_key + ".uploaded_parts"], state.uploaded_parts);
  core::Property::StringToInt(state_map[state_key + ".uploaded_size"], state.uploaded_size);
  core::Property::StringToInt(state_map[state_key + ".part_size"], state.part_size);
  core::Property::StringToInt(state_map[state_key + ".full_size"], state.full_size);
  state.uploaded_etags = minifi::utils::string::splitAndTrimRemovingEmpty(state_map[state_key + ".uploaded_etags"], ";");
  return state;
}

void MultipartUploadStateStorage::removeKey(const std::string& state_key, std::unordered_map<std::string, std::string>& state_map) {
  state_map.erase(state_key + ".upload_id");
  state_map.erase(state_key + ".upload_time");
  state_map.erase(state_key + ".uploaded_parts");
  state_map.erase(state_key + ".uploaded_size");
  state_map.erase(state_key + ".part_size");
  state_map.erase(state_key + ".full_size");
  state_map.erase(state_key + ".uploaded_etags");
}

void MultipartUploadStateStorage::removeState(const std::string& bucket, const std::string& key) {
  std::unordered_map<std::string, std::string> state_map;
  std::lock_guard<std::mutex> lock(state_manager_mutex_);
  if (!state_manager_->get(state_map)) {
    logger_->log_warn("No previous multipart upload state was associated with this processor.");
    return;
  }
  std::string state_key = bucket + "/" + key;
  if (!state_map.contains(state_key + ".upload_id")) {
    logger_->log_warn("Multipart upload state was not found for key '{}'", state_key);
    return;
  }

  removeKey(state_key, state_map);
  state_manager_->set(state_map);
  state_manager_->commit();
  state_manager_->persist();
}

void MultipartUploadStateStorage::removeAgedStates(std::chrono::milliseconds multipart_upload_max_age_threshold) {
  std::unordered_map<std::string, std::string> state_map;
  std::lock_guard<std::mutex> lock(state_manager_mutex_);
  if (!state_manager_->get(state_map)) {
    logger_->log_warn("No previous multipart upload state was associated with this processor.");
    return;
  }
  auto age_off_time = Aws::Utils::DateTime::Now() - multipart_upload_max_age_threshold;

  std::vector<std::string> keys_to_remove;
  for (const auto& [property_key, value] : state_map) {
    static constexpr std::string_view upload_time_suffix = ".upload_time";
    if (!minifi::utils::string::endsWith(property_key, upload_time_suffix)) {
      continue;
    }
    int64_t stored_upload_time{};
    if (!core::Property::StringToInt(value, stored_upload_time)) {
      logger_->log_error("Multipart upload cache key '{}' has invalid value '{}'", property_key, value);
      continue;
    }
    auto upload_time = Aws::Utils::DateTime(stored_upload_time);
    if (upload_time < age_off_time) {
      auto state_key_and_property_name = property_key.substr(0, property_key.size() - upload_time_suffix.size());
      keys_to_remove.push_back(state_key_and_property_name);
    }
  }
  for (const auto& key : keys_to_remove) {
    logger_->log_info("Removing local aged off multipart upload state with key '{}'", key);
    removeKey(key, state_map);
  }
  state_manager_->set(state_map);
  state_manager_->commit();
  state_manager_->persist();
}

}  // namespace org::apache::nifi::minifi::aws::s3
