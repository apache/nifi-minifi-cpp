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
