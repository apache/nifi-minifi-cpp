// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

  // if @cursor_str is the empty string it creates a cursor to the beginning if the underlying repository supports it
  virtual std::unique_ptr<Cursor> cursorFromString(std::string_view cursor_str) = 0;

  virtual std::expected<std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>, std::string> getEvents(size_t max_size, Cursor* cursor) = 0;
};

}  // namespace org::apache::nifi::minifi::provenance
