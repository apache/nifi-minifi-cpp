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

#include "RocksDbEncryptionProvider.h"
#include "utils/crypto/ciphers/Aes256Ecb.h"
#include "openssl/conf.h"
#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/rand.h"
#include "logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

using utils::crypto::Bytes;
using utils::crypto::Aes256EcbCipher;

namespace {

class AES256BlockCipher final : public rocksdb::BlockCipher {
  static std::shared_ptr<logging::Logger> logger_;
 public:
  AES256BlockCipher(std::string database, Aes256EcbCipher cipher_impl)
      : database_(std::move(database)),
        cipher_impl_(std::move(cipher_impl)) {}

  const char* Name() const override {
    return "AES256BlockCipher";
  }

  size_t BlockSize() override {
    return Aes256EcbCipher::BLOCK_SIZE;
  }

  rocksdb::Status Encrypt(char *data) override;

  rocksdb::Status Decrypt(char *data) override;

 private:
  const std::string database_;
  const Aes256EcbCipher cipher_impl_;
};

}  // namespace

std::shared_ptr<logging::Logger> AES256BlockCipher::logger_ = logging::LoggerFactory<AES256BlockCipher>::getLogger();

std::shared_ptr<rocksdb::EncryptionProvider> createEncryptionProvider(const utils::crypto::EncryptionManager& manager, const DbEncryptionOptions& options) {
  auto cipher = manager.createAes256EcbCipher(options.encryption_key_name);
  if (!cipher) {
    return {};
  }
  return rocksdb::EncryptionProvider::NewCTRProvider(
      std::make_shared<AES256BlockCipher>(options.database, cipher.value())
  );
}

rocksdb::Status AES256BlockCipher::Encrypt(char *data) {
  try {
    cipher_impl_.encrypt(reinterpret_cast<unsigned char*>(data));
    return rocksdb::Status::OK();
  } catch (const utils::crypto::CipherError& error) {
    logger_->log_error("Error while encrypting in database '%s': %s", database_, error.what());
    return rocksdb::Status::IOError();
  }
}

rocksdb::Status AES256BlockCipher::Decrypt(char *data) {
  try {
    cipher_impl_.decrypt(reinterpret_cast<unsigned char*>(data));
    return rocksdb::Status::OK();
  } catch (const utils::crypto::CipherError& error) {
    logger_->log_error("Error while decrypting in database '%s': %s", database_, error.what());
    return rocksdb::Status::IOError();
  }
}

}  // namespace repository
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
