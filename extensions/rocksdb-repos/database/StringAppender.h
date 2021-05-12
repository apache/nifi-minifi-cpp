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

#include "rocksdb/merge_operator.h"
#include <cstring>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

class StringAppender : public rocksdb::AssociativeMergeOperator {
 public:
  // Constructor: specify delimiter
  explicit StringAppender() = default;

  static std::shared_ptr<rocksdb::MergeOperator> transform(const std::shared_ptr<rocksdb::MergeOperator>& other) {
    if (other && std::strcmp(other->Name(), "StringAppender") == 0) {
      return other;
    }
    return std::make_shared<StringAppender>();
  }

  bool Merge(const rocksdb::Slice& /*key*/, const rocksdb::Slice* existing_value, const rocksdb::Slice& value, std::string* new_value, rocksdb::Logger* /*logger*/) const override;

  const char* Name() const override {
    return "StringAppender";
  }
};

}  // namespace repository
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
