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

#include "UnorderedMapPersistableKeyValueStoreService.h"

#include <fstream>
#include <set>

#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"

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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

constexpr int UnorderedMapPersistableKeyValueStoreService::FORMAT_VERSION;

core::Property UnorderedMapPersistableKeyValueStoreService::File(
    core::PropertyBuilder::createProperty("File")->withDescription("Path to a file to store state")
        ->isRequired(true)->build());

UnorderedMapPersistableKeyValueStoreService::UnorderedMapPersistableKeyValueStoreService(const std::string& name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : PersistableKeyValueStoreService(name, uuid)
    , AbstractAutoPersistingKeyValueStoreService(name, uuid)
    , UnorderedMapKeyValueStoreService(name, uuid)
    , logger_(logging::LoggerFactory<UnorderedMapPersistableKeyValueStoreService>::getLogger()) {
}

UnorderedMapPersistableKeyValueStoreService::UnorderedMapPersistableKeyValueStoreService(const std::string& name, const std::shared_ptr<Configure> &configuration)
    : PersistableKeyValueStoreService(name)
    , AbstractAutoPersistingKeyValueStoreService(name)
    , UnorderedMapKeyValueStoreService(name)
    , logger_(logging::LoggerFactory<UnorderedMapPersistableKeyValueStoreService>::getLogger())  {
  setConfiguration(configuration);
  initializeNonVirtual();
}

UnorderedMapPersistableKeyValueStoreService::~UnorderedMapPersistableKeyValueStoreService() {
  persistNonVirtual();
}

bool UnorderedMapPersistableKeyValueStoreService::parseLine(const std::string& line, std::string& key, std::string& value) {
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
          logger_->log_error("Invalid escape sequence in \"%s\": \"\\%c\"", line.c_str(), c);
          return false;
      }
      in_escape_sequence = false;
    } else {
      if (c == '\\') {
        in_escape_sequence = true;
      } else if (c == '=') {
        if (key_complete) {
          logger_->log_error("Unterminated \'=\' in line \"%s\"", line.c_str());
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
    logger_->log_error("Unterminated escape sequence in \"%s\"", line.c_str());
    return false;
  }
  if (!key_complete) {
    logger_->log_error("Key not found in \"%s\"", line.c_str());
    return false;
  }
  key = key_ss.str();
  if (key.empty()) {
    logger_->log_error("Line with empty key found in \"%s\": \"%s\"", file_.c_str(), line.c_str());
    return false;
  }
  value = value_ss.str();
  return true;
}

void UnorderedMapPersistableKeyValueStoreService::initializeNonVirtual() {
  AbstractAutoPersistingKeyValueStoreService::initialize();
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(File);
  updateSupportedProperties(supportedProperties);
}

void UnorderedMapPersistableKeyValueStoreService::onEnable() {
  if (configuration_ == nullptr) {
    logger_->log_debug("Cannot enable UnorderedMapPersistableKeyValueStoreService");
    return;
  }

  if (!getProperty(File.getName(), file_)) {
    logger_->log_error("Invalid or missing property: File");
    return;
  }

  /* We must not start the persistence thread until we attempted to load the state */
  load();

  AbstractAutoPersistingKeyValueStoreService::onEnable();

  logger_->log_trace("Enabled UnorderedMapPersistableKeyValueStoreService");
}

void UnorderedMapPersistableKeyValueStoreService::notifyStop() {
  AbstractAutoPersistingKeyValueStoreService::notifyStop();
  persist();
}

bool UnorderedMapPersistableKeyValueStoreService::set(const std::string& key, const std::string& value) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool res = UnorderedMapKeyValueStoreService::set(key, value);
  if (always_persist_ && res) {
    return persist();
  }
  return res;
}

bool UnorderedMapPersistableKeyValueStoreService::remove(const std::string& key) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool res = UnorderedMapKeyValueStoreService::remove(key);
  if (always_persist_ && res) {
    return persist();
  }
  return res;
}

bool UnorderedMapPersistableKeyValueStoreService::clear() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool res = UnorderedMapKeyValueStoreService::clear();
  if (always_persist_ && res) {
    return persist();
  }
  return res;
}

bool UnorderedMapPersistableKeyValueStoreService::update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool res = UnorderedMapKeyValueStoreService::update(key, update_func);
  if (always_persist_ && res) {
    return persist();
  }
  return res;
}

bool UnorderedMapPersistableKeyValueStoreService::persistNonVirtual() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::ofstream ofs(file_);
  if (!ofs.is_open()) {
    logger_->log_error("Failed to open file \"%s\" to store state", file_.c_str());
    return false;
  }
  ofs << escape(FORMAT_VERSION_KEY) << "=" << escape(std::to_string(FORMAT_VERSION)) << "\n";
  for (const auto& kv : map_) {
    ofs << escape(kv.first) << "=" << escape(kv.second) << "\n";
  }
  return true;
}

bool UnorderedMapPersistableKeyValueStoreService::load() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::ifstream ifs(file_);
  if (!ifs.is_open()) {
    logger_->log_debug("Failed to open file \"%s\" to load state", file_.c_str());
    return false;
  }
  std::unordered_map<std::string, std::string> map;
  std::string line;
  while (std::getline(ifs, line)) {
    std::string key, value;
    if (!parseLine(line, key, value)) {
      continue;
    }
    if (key == FORMAT_VERSION_KEY) {
      int format_version = 0;
      try {
        format_version = std::stoi(value);
      } catch (...) {
        logger_->log_error("Invalid format version number found in \"%s\": \"%s\"", file_.c_str(), value.c_str());
        return false;
      }
      if (format_version > FORMAT_VERSION) {
        logger_->log_error("\"%s\" has been serialized with a larger format version than currently known: %d > %d", file_.c_str(), format_version, FORMAT_VERSION);
        return false;
      }
    } else {
        map[key] = value;
    }
  }
  map_ = std::move(map);
  logger_->log_debug("Loaded state from \"%s\"", file_.c_str());
  return true;
}

REGISTER_RESOURCE(UnorderedMapPersistableKeyValueStoreService, "A persistable key-value service implemented by a locked std::unordered_map<std::string, std::string> and persisted into a file");

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
