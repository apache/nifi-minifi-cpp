// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <utility>
#include <vector>
#include "rocksdb/db.h"
#include "RocksDbUtils.h"

namespace org::apache::nifi::minifi::internal {

struct DbHandle {
  explicit DbHandle(std::unique_ptr<rocksdb::DB> handle, std::vector<DBOptionsPatch> dbo_patches)
      : dbo_patches(dbo_patches), handle(std::move(handle)) {}
  ~DbHandle();
  // we need to keep the patch object alive as the DB handle could
  // reference patcher-owned resources
  std::vector<DBOptionsPatch> dbo_patches;
  std::unique_ptr<rocksdb::DB> handle;
};

}  // namespace org::apache::nifi::minifi::internal
