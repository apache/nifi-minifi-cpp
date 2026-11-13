// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "WriteBatch.h"

namespace org::apache::nifi::minifi::internal {

rocksdb::Status WriteBatch::Put(const rocksdb::Slice &key, const rocksdb::Slice &value) {
  return impl_.Put(column_, key, value);
}

rocksdb::Status WriteBatch::Delete(const rocksdb::Slice &key) {
  return impl_.Delete(column_, key);
}

rocksdb::Status WriteBatch::Merge(const rocksdb::Slice &key, const rocksdb::Slice &value) {
  return impl_.Merge(column_, key, value);
}

}  // namespace org::apache::nifi::minifi::internal
