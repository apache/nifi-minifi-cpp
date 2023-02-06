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
#include "LookupCacher.h"

namespace org::apache::nifi::minifi::wel {

std::string LookupCacher::operator()(const std::string& key) {
  {
    std::lock_guard<std::mutex> lock{mutex_};
    const auto it = cache_.find(key);
    if (it != cache_.end() && it->second.expiry > std::chrono::system_clock::now()) {
      return it->second.value;
    }
  }

  std::string value = lookup_function_(key);

  std::lock_guard<std::mutex> lock{mutex_};
  cache_.insert_or_assign(key, CacheItem{value, std::chrono::system_clock::now() + lifetime_});
  return value;
}

}  // namespace org::apache::nifi::minifi::wel
