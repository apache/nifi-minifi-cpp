// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <optional>
#include <memory>
#include <utility>
#include "rocksdb/db.h"
#include "RocksDbUtils.h"

namespace org::apache::nifi::minifi::internal {

struct ColumnHandle {
  explicit ColumnHandle(std::unique_ptr<rocksdb::ColumnFamilyHandle> handle, ColumnFamilyOptionsPatch cfo_patch)
      : cfo_patch(cfo_patch), handle(std::move(handle)) {}
  ~ColumnHandle();
  ColumnFamilyOptionsPatch cfo_patch;
  std::unique_ptr<rocksdb::ColumnFamilyHandle> handle;
};

}  // namespace org::apache::nifi::minifi::internal
