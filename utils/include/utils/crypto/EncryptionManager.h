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
#include <filesystem>

#include "utils/crypto/EncryptionUtils.h"
#include "utils/crypto/ciphers/XSalsa20.h"
#include "utils/crypto/ciphers/Aes256Ecb.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::utils::crypto {

const auto DEFAULT_NIFI_BOOTSTRAP_FILE = std::filesystem::path("conf") / "bootstrap.conf";

class EncryptionManager {
 public:
  explicit EncryptionManager(std::filesystem::path key_dir) : key_dir_(std::move(key_dir)) {}

  template <typename Cipher> [[nodiscard]] std::optional<Cipher> getOptionalKey(const std::string& key_name) const;
  template <typename Cipher> [[nodiscard]] std::optional<Cipher> getOptionalKeyCreateIfBlank(const std::string& key_name) const;
  template <typename Cipher> [[nodiscard]] Cipher getRequiredKey(const std::string& key_name) const;

 protected:
  [[nodiscard]] virtual std::optional<Bytes> readKey(const std::string& key_name) const;
  [[nodiscard]] virtual bool writeKey(const std::string& key_name, const Bytes& key) const;

  std::filesystem::path key_dir_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<EncryptionManager>::getLogger();
};

template <typename Cipher>
std::optional<Cipher> EncryptionManager::getOptionalKey(const std::string& key_name) const {
  return readKey(key_name)
      | utils::transform([] (const Bytes& key) { return Cipher{key}; });
}

template <typename Cipher>
std::optional<Cipher> EncryptionManager::getOptionalKeyCreateIfBlank(const std::string& key_name) const {
  auto key = readKey(key_name);
  if (!key) {
    logger_->log_info("No encryption key found for '{}'", key_name);
    return std::nullopt;
  }
  if (key->empty()) {
    logger_->log_info("Generating encryption key for '{}'", key_name);
    key = Cipher::generateKey();
    if (!writeKey(key_name, key.value())) {
      logger_->log_warn("Failed to write key for '{}'", key_name);
      return std::nullopt;
    }
  } else {
    logger_->log_info("Using existing encryption key for '{}'", key_name);
  }
  return Cipher{key.value()};
}

template <typename Cipher>
Cipher EncryptionManager::getRequiredKey(const std::string& key_name) const {
  auto key = readKey(key_name);
  if (!key || key->empty()) {
    logger_->log_info("Generating encryption key for '{}'", key_name);
    key = Cipher::generateKey();
    if (!writeKey(key_name, key.value())) {
      throw Exception(GENERAL_EXCEPTION, utils::string::join_pack("Failed to write the encryption key for ", key_name, " to ", (key_dir_ / DEFAULT_NIFI_BOOTSTRAP_FILE).string()));
    }
  } else {
    logger_->log_info("Using existing encryption key for '{}'", key_name);
  }
  return Cipher{key.value()};
}

}  // namespace org::apache::nifi::minifi::utils::crypto
