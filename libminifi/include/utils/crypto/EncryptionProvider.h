/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <filesystem>

#include "utils/crypto/EncryptionUtils.h"
#include "utils/crypto/ciphers/XSalsa20.h"
#include "core/logging/Logger.h"

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
