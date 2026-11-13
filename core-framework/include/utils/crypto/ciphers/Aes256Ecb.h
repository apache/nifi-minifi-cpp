// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>
#include <utility>

#include "utils/crypto/EncryptionUtils.h"
#include "minifi-cpp/Exception.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::utils::crypto {

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

}  // namespace org::apache::nifi::minifi::utils::crypto
