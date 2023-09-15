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

#include <memory>
#include <optional>
#include <string>
#include "utils/crypto/EncryptionManager.h"
#include "properties/Properties.h"
#include "utils/StringUtils.h"
#include "utils/crypto/ciphers/XSalsa20.h"
#include "utils/crypto/ciphers/Aes256Ecb.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::utils::crypto {

const auto DEFAULT_NIFI_BOOTSTRAP_FILE = std::filesystem::path("conf") / "bootstrap.conf";

std::shared_ptr<core::logging::Logger> EncryptionManager::logger_{core::logging::LoggerFactory<EncryptionManager>::getLogger()};

std::optional<XSalsa20Cipher> EncryptionManager::createXSalsa20Cipher(const std::string &key_name) const {
  return readKey(key_name)
         | utils::transform([] (const Bytes& key) {return XSalsa20Cipher{key};});
}

std::optional<Aes256EcbCipher> EncryptionManager::createAes256EcbCipher(const std::string &key_name) const {
  auto key = readKey(key_name);
  if (!key) {
    logger_->log_info("No encryption key found for '{}'", key_name);
    return std::nullopt;
  }
  if (key->empty()) {
    // generate new key
    logger_->log_info("Generating encryption key '{}'", key_name);
    key = Aes256EcbCipher::generateKey();
    if (!writeKey(key_name, key.value())) {
      logger_->log_warn("Failed to write key '{}'", key_name);
    }
  } else {
    logger_->log_info("Using existing encryption key '{}'", key_name);
  }
  return Aes256EcbCipher{key.value()};
}


std::optional<Bytes> EncryptionManager::readKey(const std::string& key_name) const {
  minifi::Properties bootstrap_conf;
  bootstrap_conf.setHome(key_dir_);
  bootstrap_conf.loadConfigureFile(DEFAULT_NIFI_BOOTSTRAP_FILE);
  return bootstrap_conf.getString(key_name)
         | utils::transform([](const std::string &encryption_key_hex) { return utils::StringUtils::from_hex(encryption_key_hex); });
}

bool EncryptionManager::writeKey(const std::string &key_name, const Bytes& key) const {
  minifi::Properties bootstrap_conf;
  bootstrap_conf.setHome(key_dir_);
  bootstrap_conf.loadConfigureFile(DEFAULT_NIFI_BOOTSTRAP_FILE);
  bootstrap_conf.set(key_name, utils::StringUtils::to_hex(key));
  return bootstrap_conf.commitChanges();
}

}  // namespace org::apache::nifi::minifi::utils::crypto
