// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma  once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::controllers {

class InMemoryKeyValueStorage {
 public:
  InMemoryKeyValueStorage() = default;
  explicit InMemoryKeyValueStorage(std::unordered_map<std::string, std::string> contents) : map_{std::move(contents)} {}

  bool set(const std::string& key, const std::string& value);
  bool get(const std::string& key, std::string& value);
  bool get(std::unordered_map<std::string, std::string>& kvs);
  bool remove(const std::string& key);
  bool clear();
  bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func);

 private:
  std::unordered_map<std::string, std::string> map_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<InMemoryKeyValueStorage>::getLogger();
};

}  // namespace org::apache::nifi::minifi::controllers
