// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <filesystem>

#include "utils/crypto/EncryptionUtils.h"
#include "utils/crypto/ciphers/XSalsa20.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::utils::crypto {

class EncryptionProvider {
 public:
  explicit EncryptionProvider(Bytes key) : cipher_impl_(std::move(key)) {}
  explicit EncryptionProvider(XSalsa20Cipher cipher_impl) : cipher_impl_(std::move(cipher_impl)) {}

  static std::optional<EncryptionProvider> create(const std::filesystem::path& home_path);
  static EncryptionProvider createSensitivePropertiesEncryptor(const std::filesystem::path& home_path);

  [[nodiscard]] std::string encrypt(std::string_view data) const {
    return cipher_impl_.encrypt(data);
  }

  [[nodiscard]] std::string decrypt(std::string_view data) const {
    return cipher_impl_.decrypt(data);
  }

 private:
  XSalsa20Cipher cipher_impl_;
};

}  // namespace org::apache::nifi::minifi::utils::crypto
