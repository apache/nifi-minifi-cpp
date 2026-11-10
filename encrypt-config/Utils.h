// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <optional>

#include "utils/crypto/EncryptionUtils.h"

namespace org::apache::nifi::minifi::encrypt_config {

struct EncryptionKeys {
  std::optional<utils::crypto::Bytes> old_key;
  utils::crypto::Bytes encryption_key;
};

}  // namespace org::apache::nifi::minifi::encrypt_config
