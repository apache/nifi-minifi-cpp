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

#include <string>
#include <fstream>
#include "utils/file/FileSystem.h"
#include "utils/OptionalUtils.h"
#include "utils/EncryptionProvider.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

FileSystem::FileSystem(bool should_encrypt, utils::optional<utils::crypto::EncryptionProvider> encryptor)
    : should_encrypt_(should_encrypt),
      encryptor_(std::move(encryptor)) {
  if (should_encrypt_ && !encryptor) {
    throw std::invalid_argument("Requested file encryption but no encryption utility was provided");
  }
}

utils::optional<std::string> FileSystem::read(const std::string& file_name) {
  std::ifstream input{file_name, std::ios::binary};
  std::string content{std::istreambuf_iterator<char>(input), {}};
  if (!input) {
    return {};
  }
  if (encryptor_) {
    try {
      logger_->log_debug("Trying to decrypt file %s", file_name);
      content = encryptor_->decrypt(content);
    } catch(...) {
      // tried to decrypt file but failed, use file as-is
      logger_->log_debug("Decrypting file %s failed, using the file as-is", file_name);
    }
  }
  return content;
}

bool FileSystem::write(const std::string& file_name, const std::string& file_content) {
  std::ofstream output{file_name, std::ios::binary};
  if (should_encrypt_) {
    // allow a possible exception to propagate upward
    // if we fail to encrypt the file DON'T just write
    // it as-is
    logger_->log_debug("Encrypting file %s", file_name);
    output << encryptor_->encrypt(file_content);
  } else {
    logger_->log_debug("No encryption is required for file %s", file_name);
    output << file_content;
  }
  return static_cast<bool>(output);
}

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
