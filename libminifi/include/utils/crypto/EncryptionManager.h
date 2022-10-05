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
#include <utility>
#include "utils/crypto/EncryptionUtils.h"
#include "utils/crypto/ciphers/XSalsa20.h"
#include "utils/crypto/ciphers/Aes256Ecb.h"
#include "core/logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace crypto {

class EncryptionManager {
  static std::shared_ptr<core::logging::Logger> logger_;
 public:
  explicit EncryptionManager(std::string key_dir) : key_dir_(std::move(key_dir)) {}

  [[nodiscard]] std::optional<XSalsa20Cipher> createXSalsa20Cipher(const std::string& key_name) const;
  [[nodiscard]] std::optional<Aes256EcbCipher> createAes256EcbCipher(const std::string& key_name) const;
 protected:
  [[nodiscard]] virtual std::optional<Bytes> readKey(const std::string& key_name) const;
  [[nodiscard]] virtual bool writeKey(const std::string& key_name, const Bytes& key) const;

  std::string key_dir_;
};

}  // namespace crypto
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
