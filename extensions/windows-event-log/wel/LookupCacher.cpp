// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
