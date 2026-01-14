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

#include "core/logging/LoggerFactory.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "utils/Hash.h"
#include "utils/Locations.h"
#include "utils/file/FileUtils.h"

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson

namespace org::apache::nifi::minifi::utils::file {

namespace {
std::filesystem::path getRootFromConfigure(const Configure& configuration) {
  if (auto nifi_asset_directory = configuration.get(Configure::nifi_asset_directory)) {
    return *nifi_asset_directory;
  }
  return utils::getMinifiDir() / "asset";
}
}  // namespace

AssetManager::AssetManager(const Configure& configuration)
    : root_(getRootFromConfigure(configuration)),
      logger_(core::logging::LoggerFactory<AssetManager>::getLogger()) {
  refreshState();
}

void AssetManager::refreshState() {
  std::lock_guard lock(mtx_);
  state_.clear();
  if (!FileUtils::exists(root_)) {
    std::filesystem::create_directories(root_);
  }
  if (!FileUtils::exists(root_ / ".state")) {
    std::ofstream{root_ / ".state", std::ios::binary} << R"({"digest": "", "assets": {}})";
  }
  rapidjson::Document doc;

  std::string file_content = get_content(root_ / ".state");

  rapidjson::ParseResult res = doc.Parse(file_content.c_str(), file_content.size());
  if (!res) {
    logger_->log_error("Failed to parse asset '.state' file, not a valid json file");
    return;
  }
  if (!doc.IsObject()) {
    logger_->log_error("Asset '.state' file is malformed");
    return;
  }
  if (!doc.HasMember("digest")) {
    logger_->log_error("Asset '.state' file is malformed, missing 'digest'");
    return;
  }
  if (!doc["digest"].IsString()) {
    logger_->log_error("Asset '.state' file is malformed, 'digest' is not a string");
    return;
  }
  if (!doc.HasMember("assets")) {
    logger_->log_error("Asset '.state' file is malformed, missing 'assets'");
    return;
  }
  if (!doc["assets"].IsObject()) {
    logger_->log_error("Asset '.state' file is malformed, 'assets' is not an object");
    return;
  }


  AssetLayout new_state;
  new_state.digest = std::string{doc["digest"].GetString(), doc["digest"].GetStringLength()};

  for (auto& [id, entry] : doc["assets"].GetObject()) {
    if (!entry.IsObject()) {
      logger_->log_error("Asset '.state' file is malformed, 'assets.{}' is not an object", std::string_view{id.GetString(), id.GetStringLength()});
      return;
    }
    AssetDescription description;
    description.id = std::string{id.GetString(), id.GetStringLength()};
    if (!entry.HasMember("path") || !entry["path"].IsString()) {
      logger_->log_error("Asset '.state' file is malformed, 'assets.{}.path' does not exist or is not a string", std::string_view{id.GetString(), id.GetStringLength()});
      return;
    }
    description.path = std::string{entry["path"].GetString(), entry["path"].GetStringLength()};
    if (!entry.HasMember("url") || !entry["url"].IsString()) {
      logger_->log_error("Asset '.state' file is malformed, 'assets.{}.url' does not exist or is not a string", std::string_view{id.GetString(), id.GetStringLength()});
      return;
    }
    description.url = std::string{entry["url"].GetString(), entry["url"].GetStringLength()};

    if (FileUtils::exists(root_ / description.path)) {
      new_state.assets.insert(std::move(description));
    } else {
      logger_->log_error("Asset '.state' file contains entry '{}' that does not exist on the filesystem at '{}'",
                         std::string_view{id.GetString(), id.GetStringLength()}, (root_ / description.path).string());
    }
  }
  state_ = std::move(new_state);
}

std::string AssetManager::hash() const {
  std::lock_guard lock(mtx_);
  return state_.digest.empty() ? "null" : state_.digest;
}

nonstd::expected<void, std::string> AssetManager::sync(
    const AssetLayout& layout,
    const std::function<nonstd::expected<void, std::string>(std::string_view /*url*/, std::filesystem::path /*tmp_path*/)>& fetch) {
  logger_->log_info("Synchronizing assets");
  std::lock_guard lock(mtx_);
  AssetLayout new_state{
    .digest = state_.digest,
    .assets = {}
  };
  std::string fetch_errors;
  std::vector<std::filesystem::path> new_file_paths;
  for (auto& new_entry : layout.assets) {
    if (std::find_if(state_.assets.begin(), state_.assets.end(), [&] (auto& entry) {return entry.id == new_entry.id;}) == state_.assets.end()) {
      logger_->log_info("Fetching asset (id = '{}', path = '{}') from {}", new_entry.id, new_entry.path.string(), new_entry.url);
      if (auto status = fetch(new_entry.url, (root_ / new_entry.path).string() + ".part")) {
        new_file_paths.emplace_back(root_ / new_entry.path);
        new_state.assets.insert(new_entry);
      } else {
        logger_->log_error("Failed to fetch asset (id = '{}', path = '{}') from {}: {}", new_entry.id, new_entry.path.string(), new_entry.url, status.error());
        fetch_errors += "Failed to fetch '" + new_entry.id + "' from '" + new_entry.url + "': " + status.error() + "\n";
      }
    } else {
      logger_->log_info("Asset (id = '{}', path = '{}') already exists", new_entry.id, new_entry.path.string());
      new_state.assets.insert(new_entry);
    }
  }
  if (fetch_errors.empty()) {
    new_state.digest = layout.digest;
  }

  for (auto& old_entry : state_.assets) {
    if (std::find_if(layout.assets.begin(), layout.assets.end(), [&] (auto& entry) {return entry.id == old_entry.id;}) == layout.assets.end()) {
      logger_->log_info("We no longer need asset (id = '{}', path = '{}')", old_entry.id, old_entry.path.string());
      std::filesystem::remove(root_ / old_entry.path);
    }
  }

  for (auto& path : new_file_paths) {
    create_dir(path.parent_path());
    std::filesystem::rename(path.string() + ".part", path);
  }

  state_ = std::move(new_state);
  persist();

  if (!fetch_errors.empty()) {
    return nonstd::make_unexpected(fetch_errors);
  }

  return {};
}

void AssetManager::persist() const {
  std::lock_guard lock(mtx_);
  rapidjson::Document doc;
  doc.SetObject();

  doc.AddMember(rapidjson::StringRef("digest"), rapidjson::Value{state_.digest, doc.GetAllocator()}, doc.GetAllocator());
  doc.AddMember(rapidjson::StringRef("assets"), rapidjson::Value{rapidjson::kObjectType}, doc.GetAllocator());

  for (auto& entry : state_.assets) {
    rapidjson::Value entry_val(rapidjson::kObjectType);
    entry_val.AddMember(rapidjson::StringRef("path"), rapidjson::Value(entry.path.generic_string(), doc.GetAllocator()), doc.GetAllocator());
    entry_val.AddMember(rapidjson::StringRef("url"), rapidjson::StringRef(entry.url), doc.GetAllocator());
    doc["assets"].AddMember(rapidjson::StringRef(entry.id), entry_val, doc.GetAllocator());
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);

  std::ofstream{root_ / ".state", std::ios::binary}.write(buffer.GetString(), gsl::narrow<std::streamsize>(buffer.GetSize()));
}

std::filesystem::path AssetManager::getRoot() const {
  std::lock_guard lock(mtx_);
  return root_;
}

std::optional<std::filesystem::path> AssetManager::findAssetById(std::string_view id) const {
  std::lock_guard lock(mtx_);
  for (auto& asset : state_.assets) {
    if (asset.id == id) {
      return root_ / asset.path;
    }
  }
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::utils::file
