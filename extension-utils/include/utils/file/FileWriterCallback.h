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
#include <filesystem>
#include "io/StreamPipe.h"
#include "utils/expected.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::utils {

class FileWriterCallback {
 public:
  explicit FileWriterCallback(std::filesystem::path dest_path);
  ~FileWriterCallback();
  int64_t operator()(const std::shared_ptr<io::InputStream>& stream);
  bool commit();


 private:
  bool write_succeeded_ = false;
  std::filesystem::path temp_path_;
  std::filesystem::path dest_path_;
};
}  // namespace org::apache::nifi::minifi::utils
