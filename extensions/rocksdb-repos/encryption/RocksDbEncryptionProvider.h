// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>

#include "rocksdb/env_encryption.h"
#include "utils/crypto/EncryptionUtils.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "utils/crypto/EncryptionManager.h"

namespace org::apache::nifi::minifi::core::repository {

struct DbEncryptionOptions {
  std::string database;
  std::string encryption_key_name;
};

std::shared_ptr<rocksdb::Env> createEncryptingEnv(const utils::crypto::EncryptionManager& manager, const DbEncryptionOptions& options);

struct EncryptionEq {
  bool operator()(const rocksdb::Env* lhs, const rocksdb::Env* rhs) const;
};

}  // namespace org::apache::nifi::minifi::core::repository
