// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace org::apache::nifi::minifi::wel {

class LookupCacher {
 public:
  explicit LookupCacher(std::function<std::string(const std::string&)> lookup_function, std::chrono::milliseconds lifetime = std::chrono::hours{24})
    : lookup_function_(std::move(lookup_function)),
      lifetime_(lifetime) {}
  std::string operator()(const std::string& key);

 private:
  struct CacheItem {
    std::string value;
    std::chrono::system_clock::time_point expiry;
  };

  std::mutex mutex_;
  std::function<std::string(const std::string&)> lookup_function_;
  std::chrono::milliseconds lifetime_;
  std::unordered_map<std::string, CacheItem> cache_;
};

}  // namespace org::apache::nifi::minifi::wel
