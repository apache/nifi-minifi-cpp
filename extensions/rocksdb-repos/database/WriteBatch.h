// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "rocksdb/db.h"

namespace org::apache::nifi::minifi::internal {

class WriteBatch {
  friend class OpenRocksDb;
  explicit WriteBatch(rocksdb::ColumnFamilyHandle* column) : column_(column) {}
 public:
  rocksdb::Status Put(const rocksdb::Slice &key, const rocksdb::Slice &value);
  rocksdb::Status Delete(const rocksdb::Slice &key);
  rocksdb::Status Merge(const rocksdb::Slice &key, const rocksdb::Slice &value);
 private:
  rocksdb::WriteBatch impl_;
  rocksdb::ColumnFamilyHandle* column_;
};

}  // namespace org::apache::nifi::minifi::internal
