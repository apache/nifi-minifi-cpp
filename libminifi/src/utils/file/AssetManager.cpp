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

#include "utils/file/AssetManager.h"
#include "utils/file/FileUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Hash.h"

#undef GetObject

namespace org::apache::nifi::minifi::utils::file {

AssetManager::AssetManager(const Configure& configuration)
    : root_(configuration.get(Configure::nifi_asset_directory).value_or(configuration.getHome() / "asset")),
      logger_(core::logging::LoggerFactory<AssetManager>::getLogger()) {
  refreshState();
}

void AssetManager::refreshState() {
  std::lock_guard lock(mtx_);
  state_.clear();
  if (!utils::file::FileUtils::exists(root_)) {
    std::filesystem::create_directory(root_);
  }
  if (!utils::file::FileUtils::exists(root_ / ".state")) {
    std::ofstream{root_ / ".state", std::ios::binary} << "{}";
  }
  rapidjson::Document doc;

  std::string file_content = utils::file::get_content(root_ / ".state");

  rapidjson::ParseResult res = doc.Parse(file_content.c_str(), file_content.size());
  if (!res) {
    logger_->log_error("Failed to parse asset '.state' file, not a valid json file");
    return;
  }
  if (!doc.IsObject()) {
    logger_->log_error("Asset '.state' file is malformed");
    return;
  }
  AssetLayout new_state;
  for (auto& [id, entry] : doc.GetObject()) {
    if (!entry.IsObject()) {
      logger_->log_error("Asset '.state' file is malformed");
      return;
    }
    AssetDescription description;
    description.id = std::string{id.GetString(), id.GetStringLength()};
    if (!entry.HasMember("path") || !entry["path"].IsString()) {
      logger_->log_error("Asset '.state' file is malformed");
      return;
    }
    description.path = std::string{entry["path"].GetString(), entry["path"].GetStringLength()};
    if (!entry.HasMember("url") || !entry["url"].IsString()) {
      logger_->log_error("Asset '.state' file is malformed");
      return;
    }
    description.url = std::string{entry["url"].GetString(), entry["url"].GetStringLength()};

    if (utils::file::FileUtils::exists(root_ / description.id)) {
      new_state.insert(std::move(description));
    } else {
      logger_->log_error("Asset '.state' file contains entry that does not exist on the filesystem");
    }
  }
  state_ = std::move(new_state);
}

std::string AssetManager::hash() const {
  std::lock_guard lock(mtx_);
  if (!cached_hash_) {
    size_t hash_value{0};
    for (auto& entry : state_) {
      hash_value = utils::hash_combine(hash_value, std::hash<std::string>{}(entry.id));
    }
    cached_hash_ = std::to_string(hash_value);
  }
  return cached_hash_.value();
}

nonstd::expected<void, std::string> AssetManager::sync(
    org::apache::nifi::minifi::utils::file::AssetLayout layout,
    std::function<nonstd::expected<std::vector<std::byte>, std::string>(std::string_view /*url*/)> fetch) {
  std::lock_guard lock(mtx_);
  std::vector<std::pair<std::filesystem::path, std::vector<std::byte>>> new_file_contents;
  for (auto& new_entry : layout) {
    if (std::find_if(state_.begin(), state_.end(), [&] (auto& entry) {return entry.id == new_entry.id;}) == state_.end()) {
      if (auto data = fetch(new_entry.url)) {
        new_file_contents.emplace_back(new_entry.path, data.value());
      } else {
        return nonstd::make_unexpected(data.error());
      }
    }
  }
  cached_hash_.reset();

  for (auto& old_entry : state_) {
    if (std::find_if(layout.begin(), layout.end(), [&] (auto& entry) {return entry.id == old_entry.id;}) == layout.end()) {
      // we no longer need this asset
      std::filesystem::remove(root_ / old_entry.path);
    }
  }

  for (auto& [path, content] : new_file_contents) {
    utils::file::create_dir((root_ / path).parent_path());
    std::ofstream{root_ / path, std::ios::binary}.write(reinterpret_cast<const char*>(content.data()), content.size());
  }

  state_ = std::move(layout);
  persist();
  return {};
}

void AssetManager::persist() const {
  std::lock_guard lock(mtx_);
  rapidjson::Document doc;
  doc.SetObject();

  for (auto& entry : state_) {
    rapidjson::Value entry_val(rapidjson::kObjectType);
    entry_val.AddMember(rapidjson::StringRef("path"), rapidjson::Value(entry.path.string().c_str(), doc.GetAllocator()), doc.GetAllocator());
    entry_val.AddMember(rapidjson::StringRef("url"), rapidjson::StringRef(entry.url), doc.GetAllocator());
    doc.AddMember(rapidjson::StringRef(entry.id), entry_val, doc.GetAllocator());
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);

  std::ofstream{root_ / ".state", std::ios::binary}.write(buffer.GetString(), buffer.GetSize());
}

std::filesystem::path AssetManager::getRoot() const {
  std::lock_guard lock(mtx_);
  return root_;
}

}  // namespace org::apache::nifi::minifi::utils::file
