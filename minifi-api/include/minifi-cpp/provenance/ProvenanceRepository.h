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

#include "minifi-cpp/core/Repository.h"
#include "utils/Id.h"
#include "minifi-cpp/provenance/Provenance.h"

namespace org::apache::nifi::minifi::provenance {

class ProvenanceRepository : public virtual core::Repository {
 public:
  class Cursor {
  public:
    [[nodiscard]]
    virtual std::string toString() const = 0;
    virtual ~Cursor() = default;
  };

  virtual std::expected<void, std::string> appendEvents(const std::vector<std::shared_ptr<ProvenanceEventRecord>>& events) = 0;

  // if @cursor_str is nullopt it creates a cursor to the beginning if the underlying repository supports it
  virtual std::unique_ptr<Cursor> cursorFromString(std::optional<std::string> cursor_str) = 0;

  virtual std::expected<std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>, std::string> getEvents(size_t max_size, Cursor* cursor) = 0;
};

}  // namespace org::apache::nifi::minifi::provenance
