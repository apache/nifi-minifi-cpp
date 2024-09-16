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
#include "utils/crypto/EncryptionProvider.h"
#include "core/logging/LoggerFactory.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

class FileSystem {
 public:
  explicit FileSystem(bool should_encrypt = false, std::optional<utils::crypto::EncryptionProvider> encryptor = {});

  FileSystem(const FileSystem&) = delete;
  FileSystem(FileSystem&&) = delete;
  FileSystem& operator=(const FileSystem&) = delete;
  FileSystem& operator=(FileSystem&&) = delete;

  std::optional<std::string> read(const std::filesystem::path& file_name);

  bool write(const std::filesystem::path& file_name, const std::string& file_content);

 private:
  bool should_encrypt_on_write_;
  std::optional<utils::crypto::EncryptionProvider> encryptor_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<FileSystem>::getLogger()};
};

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
