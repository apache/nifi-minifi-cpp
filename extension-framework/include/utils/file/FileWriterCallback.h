// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <filesystem>
#include "io/StreamPipe.h"
#include "utils/expected.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::utils {

class FileWriterCallback {
 public:
  explicit FileWriterCallback(std::filesystem::path dest_path);
  ~FileWriterCallback();
  io::IoResult operator()(const std::shared_ptr<io::InputStream>& stream);
  bool commit();


 private:
  bool write_succeeded_ = false;
  std::filesystem::path temp_path_;
  std::filesystem::path dest_path_;
};
}  // namespace org::apache::nifi::minifi::utils
