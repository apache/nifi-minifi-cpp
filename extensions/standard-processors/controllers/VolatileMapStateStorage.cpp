// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "VolatileMapStateStorage.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::controllers {

void VolatileMapStateStorage::initialize() {
  setSupportedProperties(Properties);
}

bool VolatileMapStateStorage::set(const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.set(key, value);
}

bool VolatileMapStateStorage::get(const std::string& key, std::string& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.get(key, value);
}

bool VolatileMapStateStorage::get(std::unordered_map<std::string, std::string>& kvs) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.get(kvs);
}

bool VolatileMapStateStorage::remove(const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.remove(key);
}

bool VolatileMapStateStorage::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.clear();
}

bool VolatileMapStateStorage::update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) {
  std::lock_guard<std::mutex> lock(mutex_);
  return storage_.update(key, update_func);
}

REGISTER_RESOURCE_AS(VolatileMapStateStorage, ControllerService, ("UnorderedMapKeyValueStoreService", "VolatileMapStateStorage"));

}  // namespace org::apache::nifi::minifi::controllers
