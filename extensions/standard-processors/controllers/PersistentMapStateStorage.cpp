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

#include "PersistentMapStateStorage.h"

#include <cinttypes>
#include <fstream>
#include <set>

#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"
#include "core/Resource.h"

namespace {
  std::string escape(const std::string& str) {
    std::stringstream escaped;
    for (const auto c : str) {
      switch (c) {
        case '\\':
          escaped << "\\\\";
          break;
        case '\n':
          escaped << "\\n";
          break;
        case '=':
          escaped << "\\=";
          break;
        default:
          escaped << c;
          break;
      }
    }
    return escaped.str();
  }
}  // namespace

namespace org::apache::nifi::minifi::controllers {

PersistentMapStateStorage::PersistentMapStateStorage(const std::string& name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : KeyValueStateStorage(name, uuid) {
}

PersistentMapStateStorage::PersistentMapStateStorage(const std::string& name, const std::shared_ptr<Configure> &configuration)
    : KeyValueStateStorage(name) {
  setConfiguration(configuration);
}

PersistentMapStateStorage::~PersistentMapStateStorage() {
  auto_persistor_.stop();
  persistNonVirtual();
}

bool PersistentMapStateStorage::parseLine(const std::string& line, std::string& key, std::string& value) {
  std::stringstream key_ss;
  std::stringstream value_ss;
  bool in_escape_sequence = false;
  bool key_complete = false;
  for (const auto c : line) {
    auto& current = key_complete ? value_ss : key_ss;
    if (in_escape_sequence) {
      switch (c) {
        case '\\':
          current << '\\';
          break;
        case 'n':
          current << '\n';
          break;
        case '=':
          current << '=';
          break;
        default:
          logger_->log_error(R"(Invalid escape sequence in "{}": "\{}")", line.c_str(), c);
          return false;
      }
      in_escape_sequence = false;
    } else {
      if (c == '\\') {
        in_escape_sequence = true;
      } else if (c == '=') {
        if (key_complete) {
          logger_->log_error(R"(Unterminated '=' in line "{}")", line.c_str());
          return false;
        } else {
          key_complete = true;
        }
      } else {
        current << c;
      }
    }
  }
  if (in_escape_sequence) {
    logger_->log_error("Unterminated escape sequence in \"{}\"", line.c_str());
    return false;
  }
  if (!key_complete) {
    logger_->log_error("Key not found in \"{}\"", line.c_str());
    return false;
  }
  key = key_ss.str();
  if (key.empty()) {
    logger_->log_error(R"(Line with empty key found in "{}": "{}")", file_.c_str(), line.c_str());
    return false;
  }
  value = value_ss.str();
  return true;
}

void PersistentMapStateStorage::initialize() {
  // VolatileMapStateStorage::initialize() also calls setSupportedProperties, and we don't want that
  ControllerServiceImpl::initialize();  // NOLINT(bugprone-parent-virtual-call)
  setSupportedProperties(Properties);
}

void PersistentMapStateStorage::onEnable() {
  if (configuration_ == nullptr) {
    logger_->log_debug("Cannot enable PersistentMapStateStorage");
    return;
  }

  const auto always_persist = getProperty<bool>(AlwaysPersist).value_or(false);
  logger_->log_info("Always Persist property: {}", always_persist);

  const auto auto_persistence_interval = getProperty<core::TimePeriodValue>(AutoPersistenceInterval).value_or(core::TimePeriodValue{}).getMilliseconds();
  logger_->log_info("Auto Persistence Interval property: {}", auto_persistence_interval);

  if (!getProperty(File, file_)) {
    logger_->log_error("Invalid or missing property: File");
    return;
  }

  /* We must not start the persistence thread until we attempted to load the state */
  load();

  auto_persistor_.start(always_persist, auto_persistence_interval, [this] { return persistNonVirtual(); });

  logger_->log_trace("Enabled PersistentMapStateStorage");
}

void PersistentMapStateStorage::notifyStop() {
  auto_persistor_.stop();
  persist();
}

bool PersistentMapStateStorage::set(const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  bool res = storage_.set(key, value);
  if (auto_persistor_.isAlwaysPersisting() && res) {
    return persist();
  }
  return res;
}

bool PersistentMapStateStorage::get(const std::string& key, std::string& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.get(key, value);
}

bool PersistentMapStateStorage::get(std::unordered_map<std::string, std::string>& kvs) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.get(kvs);
}

bool PersistentMapStateStorage::remove(const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  bool res = storage_.remove(key);
  if (auto_persistor_.isAlwaysPersisting() && res) {
    return persist();
  }
  return res;
}

bool PersistentMapStateStorage::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  bool res = storage_.clear();
  if (auto_persistor_.isAlwaysPersisting() && res) {
    return persist();
  }
  return res;
}

bool PersistentMapStateStorage::update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) {
  std::lock_guard<std::mutex> lock(mutex_);
  bool res = storage_.update(key, update_func);
  if (auto_persistor_.isAlwaysPersisting() && res) {
    return persist();
  }
  return res;
}

bool PersistentMapStateStorage::persistNonVirtual() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ofstream ofs(file_);
  if (!ofs.is_open()) {
    logger_->log_error("Failed to open file \"{}\" to store state", file_.c_str());
    return false;
  }
  std::unordered_map<std::string, std::string> storage_copy;
  if (!storage_.get(storage_copy)) {
    logger_->log_error("Could not read the contents of the in-memory storage");
    return false;
  }

  ofs << escape(FORMAT_VERSION_KEY) << "=" << escape(std::to_string(FORMAT_VERSION)) << "\n";

  for (const auto& kv : storage_copy) {
    ofs << escape(kv.first) << "=" << escape(kv.second) << "\n";
  }
  return true;
}

bool PersistentMapStateStorage::load() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ifstream ifs(file_);
  if (!ifs.is_open()) {
    logger_->log_debug("Failed to open file \"{}\" to load state", file_.c_str());
    return false;
  }
  std::unordered_map<std::string, std::string> map;
  std::string line;
  while (std::getline(ifs, line)) {
    std::string key;
    std::string value;
    if (!parseLine(line, key, value)) {
      continue;
    }
    if (key == FORMAT_VERSION_KEY) {
      int format_version = 0;
      try {
        format_version = std::stoi(value);
      } catch (...) {
        logger_->log_error(R"(Invalid format version number found in "{}": "{}")", file_.c_str(), value.c_str());
        return false;
      }
      if (format_version > FORMAT_VERSION) {
        logger_->log_error("\"{}\" has been serialized with a larger format version than currently known: {} > {}", file_.c_str(), format_version, FORMAT_VERSION);
        return false;
      }
    } else {
        map[key] = value;
    }
  }

  storage_ = InMemoryKeyValueStorage{std::move(map)};
  logger_->log_debug("Loaded state from \"{}\"", file_.c_str());
  return true;
}

REGISTER_RESOURCE_AS(PersistentMapStateStorage, ControllerService, ("UnorderedMapPersistableKeyValueStoreService", "PersistentMapStateStorage"));

}  // namespace org::apache::nifi::minifi::controllers
