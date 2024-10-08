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

#include <string>
#include <memory>
#include <utility>

#include "utils/crypto/EncryptionUtils.h"
#include "Exception.h"
#include "core/logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace crypto {

class CipherError : public Exception {
 public:
  explicit CipherError(const std::string& error_msg) : Exception(ExceptionType::GENERAL_EXCEPTION, error_msg) {}
};

/**
 * This cipher in itself is unsafe to use to encrypt sensitive data.
 * Consider this cipher as a building block for more secure modes
 * of operations (CTR, CBC, etc.)
 */
class Aes256EcbCipher {
  static std::shared_ptr<core::logging::Logger> logger_;

 public:
  static constexpr size_t BLOCK_SIZE = 16;
  static constexpr size_t KEY_SIZE = 32;

  explicit Aes256EcbCipher(Bytes encryption_key);
  void encrypt(std::span<unsigned char, BLOCK_SIZE> data) const;
  void decrypt(std::span<unsigned char, BLOCK_SIZE> data) const;

  static Bytes generateKey();

  bool operator==(const Aes256EcbCipher& other) const;

 private:
  static void handleError(const std::string& error_msg) {
    logger_->log_error("{}", error_msg);
    throw CipherError(error_msg);
  }

  static void handleOpenSSLError(const char* msg);

  const Bytes encryption_key_;
};

}  // namespace crypto
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
