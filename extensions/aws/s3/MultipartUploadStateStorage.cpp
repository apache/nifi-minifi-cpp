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

void MultipartUploadStateStorage::storeState(const std::string& bucket, const std::string& key, const MultipartUploadState& state) {
  std::unordered_map<std::string, std::string> stored_state;
  state_manager_->get(stored_state);
  std::string state_key = bucket + "/" + key;
  stored_state[state_key + ".upload_id"] = state.upload_id;
  stored_state[state_key + ".upload_time"] = std::to_string(state.upload_time.Millis());
  stored_state[state_key + ".uploaded_parts"] = std::to_string(state.uploaded_parts);
  stored_state[state_key + ".uploaded_size"] = std::to_string(state.uploaded_size);
  stored_state[state_key + ".part_size"] = std::to_string(state.part_size);
  stored_state[state_key + ".full_size"] = std::to_string(state.full_size);
  stored_state[state_key + ".uploaded_etags"] = minifi::utils::StringUtils::join(";", state.uploaded_etags);
  state_manager_->set(stored_state);
  state_manager_->persist();
}

std::optional<MultipartUploadState> MultipartUploadStateStorage::getState(const std::string& bucket, const std::string& key) const {
  std::unordered_map<std::string, std::string> state_map;
  if (!state_manager_->get(state_map)) {
    logger_->log_warn("No previous multipart upload state was associated with this processor.");
    return std::nullopt;
  }
  std::string state_key = bucket + "/" + key;
  if (!state_map.contains(state_key + ".upload_id")) {
    logger_->log_warn("Multipart upload state was not found for key '%s'", state_key);
    return std::nullopt;
  }

  MultipartUploadState state;
  state.upload_id = state_map[state_key + ".upload_id"];

  int64_t stored_upload_time = 0;
  core::Property::StringToInt(state_map[state_key + ".upload_time"], stored_upload_time);
  state.upload_time = Aws::Utils::DateTime(stored_upload_time);

  core::Property::StringToInt(state_map[state_key + ".uploaded_parts"], state.uploaded_parts);
  core::Property::StringToInt(state_map[state_key + ".uploaded_size"], state.uploaded_size);
  core::Property::StringToInt(state_map[state_key + ".part_size"], state.part_size);
  core::Property::StringToInt(state_map[state_key + ".full_size"], state.full_size);
  state.uploaded_etags = minifi::utils::StringUtils::splitAndTrimRemovingEmpty(state_map[state_key + ".uploaded_etags"], ";");
  return state;
}

void MultipartUploadStateStorage::removeState(const std::string& bucket, const std::string& key) {
  std::unordered_map<std::string, std::string> state_map;
  if (!state_manager_->get(state_map)) {
    logger_->log_warn("No previous multipart upload state was associated with this processor.");
    return;
  }
  std::string state_key = bucket + "/" + key;
  if (!state_map.contains(state_key + ".upload_id")) {
    logger_->log_warn("Multipart upload state was not found for key '%s'", state_key);
    return;
  }

  state_map.erase(state_key + ".upload_id");
  state_map.erase(state_key + ".upload_time");
  state_map.erase(state_key + ".uploaded_parts");
  state_map.erase(state_key + ".uploaded_size");
  state_map.erase(state_key + ".part_size");
  state_map.erase(state_key + ".full_size");
  state_map.erase(state_key + ".uploaded_etags");
  state_manager_->set(state_map);
  state_manager_->persist();
}

}  // namespace org::apache::nifi::minifi::aws::s3
