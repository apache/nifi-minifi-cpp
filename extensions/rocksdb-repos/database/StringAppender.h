// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
