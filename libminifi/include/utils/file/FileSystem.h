// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <optional>
#include <string>
#include "utils/crypto/EncryptionProvider.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::utils::file {

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

}  // namespace org::apache::nifi::minifi::utils::file
