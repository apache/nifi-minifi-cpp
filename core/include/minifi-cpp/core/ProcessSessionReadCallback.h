/**
 * @file ProcessSessionReadCallback.h
 * ProcessSessionReadCallback class declaration
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
#pragma once

#include <memory>
#include <string>

#include "core/logging/LoggerFactory.h"
#include "FlowFileRecord.h"
#include "io/InputStream.h"

namespace org::apache::nifi::minifi::core {
class ProcessSessionReadCallback {
 public:
  ProcessSessionReadCallback(std::filesystem::path temp_file, std::filesystem::path dest_file, std::shared_ptr<logging::Logger> logger);
  ~ProcessSessionReadCallback();
  int64_t operator()(const std::shared_ptr<io::InputStream>& stream);
  bool commit();

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::ofstream tmp_file_os_;
  bool write_succeeded_ = false;
  std::filesystem::path tmp_file_;
  std::filesystem::path dest_file_;
};
}  // namespace org::apache::nifi::minifi::core
