// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "InMemoryKeyValueStorage.h"

namespace org::apache::nifi::minifi::controllers {

bool InMemoryKeyValueStorage::set(const std::string& key, const std::string& value) {
  map_[key] = value;
  return true;
}

bool InMemoryKeyValueStorage::get(const std::string& key, std::string& value) {
  auto it = map_.find(key);
  if (it == map_.end()) {
    return false;
  } else {
    value = it->second;
    return true;
  }
}

bool InMemoryKeyValueStorage::get(std::unordered_map<std::string, std::string>& kvs) {
  kvs = map_;
  return true;
}

bool InMemoryKeyValueStorage::remove(const std::string& key) {
  return map_.erase(key) == 1U;
}

bool InMemoryKeyValueStorage::clear() {
  map_.clear();
  return true;
}

bool InMemoryKeyValueStorage::update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) {
  bool exists = false;
  std::string value;
  auto it = map_.find(key);
  if (it != map_.end()) {
    exists = true;
    value = it->second;
  }
  try {
    if (!update_func(exists, value)) {
      return false;
    }
  } catch (const std::exception& e) {
    logger_->log_error("update_func failed with an exception: {}", e.what());
    return false;
  } catch (...) {
    logger_->log_error("update_func failed with an exception");
    return false;
  }
  if (!exists) {
    it = map_.emplace(key, "").first;
  }
  it->second = std::move(value);
  return true;
}

}  // namespace org::apache::nifi::minifi::controllers
