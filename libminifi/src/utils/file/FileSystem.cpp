// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <fstream>
#include <string>
#include "utils/file/FileSystem.h"
#include "utils/crypto/EncryptionProvider.h"

namespace org::apache::nifi::minifi::utils::file {

FileSystem::FileSystem(bool should_encrypt_on_write, std::optional<utils::crypto::EncryptionProvider> encryptor)
    : should_encrypt_on_write_(should_encrypt_on_write),
      encryptor_(std::move(encryptor)) {
  if (should_encrypt_on_write_ && !encryptor_) {
    std::string err_message = "Requested file encryption but no encryption utility was provided";
    logger_->log_error("{}", err_message);
    throw std::invalid_argument(err_message);
  }
}

std::optional<std::string> FileSystem::read(const std::filesystem::path& file_name) {
  std::ifstream input{file_name, std::ios::binary};
  if (!input) {
    return std::nullopt;
  }
  input.exceptions(std::ios::failbit | std::ios::badbit);
  std::string content{std::istreambuf_iterator<char>(input), {}};
  if (encryptor_) {
    try {
      logger_->log_debug("Trying to decrypt file {}", file_name);
      content = encryptor_->decrypt(content);
    } catch(...) {
      // tried to decrypt file but failed, use file as-is
      logger_->log_debug("Decrypting file {} failed, using the file as-is", file_name);
    }
  }
  return content;
}

bool FileSystem::write(const std::filesystem::path& file_name, const std::string& file_content) {
  std::ofstream output{file_name, std::ios::binary};
  if (should_encrypt_on_write_) {
    // allow a possible exception to propagate upward
    // if we fail to encrypt the file DON'T just write
    // it as-is
    logger_->log_debug("Encrypting file {}", file_name);
    output << encryptor_->encrypt(file_content);
  } else {
    logger_->log_debug("No encryption is required for file {}", file_name);
    output << file_content;
  }
  output.close();
  return static_cast<bool>(output);
}

}  // namespace org::apache::nifi::minifi::utils::file
