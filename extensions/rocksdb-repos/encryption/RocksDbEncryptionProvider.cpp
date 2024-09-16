/**
 *
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

#include <string>
#include <utility>
#include <memory>

#include "RocksDbEncryptionProvider.h"
#include "utils/crypto/ciphers/Aes256Ecb.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core::repository {

using utils::crypto::Bytes;
using utils::crypto::Aes256EcbCipher;

/**
 * This cipher is used by rocksdb to implement a CTR encryption scheme.
 */
class AES256BlockCipher final : public rocksdb::BlockCipher {
  static std::shared_ptr<logging::Logger> logger_;

 public:
  AES256BlockCipher(std::string database, Aes256EcbCipher cipher_impl)
      : database_(std::move(database)),
        cipher_impl_(std::move(cipher_impl)) {}

  [[nodiscard]] const char *Name() const override {
    return "AES256BlockCipher";
  }

  size_t BlockSize() override {
    return Aes256EcbCipher::BLOCK_SIZE;
  }

  bool operator==(const AES256BlockCipher& other) const {
    return cipher_impl_ == other.cipher_impl_;
  }

  rocksdb::Status Encrypt(char *data) override;

  rocksdb::Status Decrypt(char *data) override;

 private:
  const std::string database_;
  const Aes256EcbCipher cipher_impl_;
};

class EncryptingEnv : public rocksdb::EnvWrapper {
 public:
  EncryptingEnv(Env* target, std::shared_ptr<AES256BlockCipher> cipher) : EnvWrapper(target), env_(target), cipher_(std::move(cipher)) {}

  [[nodiscard]] bool hasEqualKey(const EncryptingEnv& other) const {
    return *cipher_ == *other.cipher_;
  }

 private:
  std::unique_ptr<Env> env_;
  std::shared_ptr<AES256BlockCipher> cipher_;
};

std::shared_ptr<logging::Logger> AES256BlockCipher::logger_ = logging::LoggerFactory<AES256BlockCipher>::getLogger();

std::shared_ptr<rocksdb::Env> createEncryptingEnv(const utils::crypto::EncryptionManager& manager, const DbEncryptionOptions& options) {
  auto cipher_impl = manager.getOptionalKeyCreateIfBlank<Aes256EcbCipher>(options.encryption_key_name);
  if (!cipher_impl) {
    return {};
  }
  auto cipher = std::make_shared<AES256BlockCipher>(options.database, cipher_impl.value());
  return std::make_shared<EncryptingEnv>(
      rocksdb::NewEncryptedEnv(rocksdb::Env::Default(), rocksdb::EncryptionProvider::NewCTRProvider(cipher)), cipher);
}

rocksdb::Status AES256BlockCipher::Encrypt(char *data) {
  try {
    cipher_impl_.encrypt(std::span<unsigned char, Aes256EcbCipher::BLOCK_SIZE>{reinterpret_cast<unsigned char*>(data), Aes256EcbCipher::BLOCK_SIZE});
    return rocksdb::Status::OK();
  } catch (const utils::crypto::CipherError& error) {
    logger_->log_error("Error while encrypting in database '{}': {}", database_, error.what());
    return rocksdb::Status::IOError();
  }
}

rocksdb::Status AES256BlockCipher::Decrypt(char *data) {
  try {
    cipher_impl_.decrypt(std::span<unsigned char, Aes256EcbCipher::BLOCK_SIZE>{reinterpret_cast<unsigned char*>(data), Aes256EcbCipher::BLOCK_SIZE});
    return rocksdb::Status::OK();
  } catch (const utils::crypto::CipherError& error) {
    logger_->log_error("Error while decrypting in database '{}': {}", database_, error.what());
    return rocksdb::Status::IOError();
  }
}

bool EncryptionEq::operator()(const rocksdb::Env* lhs, const rocksdb::Env* rhs) const {
  auto* lhs_enc = dynamic_cast<const EncryptingEnv*>(lhs);
  auto* rhs_enc = dynamic_cast<const EncryptingEnv*>(rhs);
  if (lhs_enc == rhs_enc) return true;
  if (!lhs_enc || !rhs_enc) return false;
  return lhs_enc->hasEqualKey(*rhs_enc);
}

}  // namespace org::apache::nifi::minifi::core::repository
