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

#include "couchbase/upsert_options.hxx"
#include "nonstd/expected.hpp"

namespace org::apache::nifi::minifi::couchbase {

struct CouchbaseUpsertResult {
  std::string bucket_name;
  std::uint64_t cas{0};
  std::uint64_t sequence_number{0};
  std::uint64_t partition_uuid{0};
  std::uint16_t partition_id{0};
};

class CouchbaseCollection {
 public:
  virtual nonstd::expected<CouchbaseUpsertResult, std::error_code> upsert(const std::string& document_id, const std::vector<std::byte>& buffer, const ::couchbase::upsert_options& options) = 0;
  virtual ~CouchbaseCollection() = default;
};

}  // namespace org::apache::nifi::minifi::couchbase
