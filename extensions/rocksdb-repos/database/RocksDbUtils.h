/**
 *
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

#include <functional>
#include <algorithm>
#include <optional>
#include <memory>
#include <string>
#include <unordered_map>

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "utils/GeneralUtils.h"
#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::internal {

enum class RocksDbMode {
  ReadOnly,
  ReadWrite
};

template<typename T>
class Writable {
 public:
  explicit Writable(T& target) : target_(target) {}

  template<typename F, typename Comparator = std::equal_to<F>>
  void set(F T::* member, typename utils::type_identity<F>::type value, const Comparator& comparator = Comparator{}) {
    if (!comparator(target_.*member, value)) {
      target_.*member = value;
      is_modified_ = true;
    }
  }

  template <typename Method, typename... Args>
  decltype(auto) call(Method method, Args&&... args) {
    return std::invoke(method, target_, std::forward<Args>(args)...);
  }

  void optimizeForSmallDb(std::shared_ptr<rocksdb::Cache> cache, std::shared_ptr<rocksdb::WriteBufferManager> wbm) {
    if (!cache || !wbm) { return; }
    target_.OptimizeForSmallDb(&cache);
    target_.write_buffer_manager = wbm;
    target_.max_open_files = 20;
  }

  template<typename F>
  const F& get(F T::* member) {
    return target_.*member;
  }

  bool isModified() const noexcept {
    return is_modified_;
  }

 private:
  bool is_modified_{false};
  T& target_;
};

/**
 * Purpose: unfortunately a default constructed database is reported "not compatible"
 * when checking against default constructed DBOptions. So we first need to query the
 * options of the existing database, apply a "patch" and then check for compatibility.
 */
using DBOptionsPatch = std::function<void(Writable<rocksdb::DBOptions>&)>;
using ColumnFamilyOptionsPatch = std::function<void(rocksdb::ColumnFamilyOptions&)>;

std::optional<rocksdb::CompressionType> readConfiguredCompressionType(const std::shared_ptr<Configure> &configuration, const std::string& config_key);
void setCommonRocksDbOptions(Writable<rocksdb::DBOptions>& db_opts);
std::unordered_map<std::string, std::string> getRocksDbOptionsToOverride(const std::shared_ptr<Configure> &configuration, std::string_view custom_db_prefix);

}  // namespace org::apache::nifi::minifi::internal
