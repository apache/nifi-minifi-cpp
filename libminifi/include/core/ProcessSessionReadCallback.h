// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>

#include "core/logging/LoggerFactory.h"
#include "FlowFileRecord.h"
#include "minifi-cpp/io/InputStream.h"

namespace org::apache::nifi::minifi::core {
class ProcessSessionReadCallback {
 public:
  ProcessSessionReadCallback(std::filesystem::path temp_file, std::filesystem::path dest_file, std::shared_ptr<logging::Logger> logger);
  ~ProcessSessionReadCallback();
  io::IoResult operator()(const std::shared_ptr<io::InputStream>& stream);
  bool commit();

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::ofstream tmp_file_os_;
  bool write_succeeded_ = false;
  std::filesystem::path tmp_file_;
  std::filesystem::path dest_file_;
};
}  // namespace org::apache::nifi::minifi::core
