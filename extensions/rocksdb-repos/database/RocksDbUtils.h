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
#include "rocksdb/db.h"
#include "utils/GeneralUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

enum class RocksDbMode {
  ReadOnly,
  ReadWrite
};

template<typename T>
class Writable {
 public:
  explicit Writable(T& target) : target_(target) {}

  template<typename F>
  void set(F T::* member, typename utils::type_identity<F>::type value) {
    if (!(target_.*member == value)) {
      target_.*member = value;
      is_modified_ = true;
    }
  }

  template<typename Transformer, typename F>
  void transform(F T::* member) {
    set(member, Transformer::transform(target_.*member));
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
using ColumnFamilyOptionsPatch = std::function<void(Writable<rocksdb::ColumnFamilyOptions>&)>;

}  // namespace internal
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
