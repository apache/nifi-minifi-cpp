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

#include <cstring>
#include <string>
#include <memory>
#include <algorithm>
#include "rocksdb/merge_operator.h"

namespace org::apache::nifi::minifi::core::repository {

class StringAppender : public rocksdb::AssociativeMergeOperator {
 public:
  struct Eq {
    bool operator()(const std::shared_ptr<rocksdb::MergeOperator>& lhs, const std::shared_ptr<rocksdb::MergeOperator>& rhs) const {
      if (lhs == rhs) return true;
      if (!lhs || !rhs) return false;
      return std::strcmp(lhs->Name(), rhs->Name()) == 0;
    }
  };

  bool Merge(const rocksdb::Slice& /*key*/, const rocksdb::Slice* existing_value, const rocksdb::Slice& value, std::string* new_value, rocksdb::Logger* /*logger*/) const override;

  const char* Name() const override {
    return "StringAppender";
  }
};

}  // namespace org::apache::nifi::minifi::core::repository
