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

#include "StringAppender.h"
#include "rocksdb/utilities/object_registry.h"

namespace org::apache::nifi::minifi::core::repository {

bool StringAppender::Merge(const rocksdb::Slice& /*key*/, const rocksdb::Slice* existing_value, const rocksdb::Slice& value, std::string* new_value, rocksdb::Logger* /*logger*/) const {
  // Clear the *new_value for writing.
  if (nullptr == new_value) {
    return false;
  }
  new_value->clear();

  if (!existing_value) {
    // No existing_value. Set *new_value = value
    new_value->assign(value.data(), value.size());
  } else {
    new_value->reserve(existing_value->size() + value.size());
    new_value->assign(existing_value->data(), existing_value->size());
    new_value->append(value.data(), value.size());
  }

  return true;
}

static auto string_appender_registrar = rocksdb::ObjectLibrary::Default()->Register<StringAppender>(
    "StringAppender",
    [] (const std::string& /* uri */, std::unique_ptr<StringAppender>* out, std::string* /* errmsg */) {
      *out = std::make_unique<StringAppender>();
      return out->get();
    });

}  // namespace org::apache::nifi::minifi::core::repository
