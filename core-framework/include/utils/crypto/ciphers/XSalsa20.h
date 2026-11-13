// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <utility>

#include "utils/crypto/EncryptionUtils.h"

namespace org::apache::nifi::minifi::utils::crypto {

class XSalsa20Cipher {
 public:
  explicit XSalsa20Cipher(Bytes encryption_key) : encryption_key_(std::move(encryption_key)) {}

  static Bytes generateKey() {
    return utils::crypto::generateKey();
  }

  [[nodiscard]] std::string encrypt(std::string_view data) const {
    return utils::crypto::encrypt(data, encryption_key_);
  }

  [[nodiscard]] std::string decrypt(std::string_view data) const {
    return utils::crypto::decrypt(data, encryption_key_);
  }

 private:
  Bytes encryption_key_;
};

}  // namespace org::apache::nifi::minifi::utils::crypto
