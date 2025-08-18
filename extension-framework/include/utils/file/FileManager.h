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
#include <vector>
#include <cstdlib>
#include <cstdio>

#include "io/validation.h"
#include "utils/Id.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::utils::file {

/**
 * Simple implementation of simple file manager utilities.
 *
 * unique_file is not a static implementation so that we can support scope driven temporary files.
 */
class FileManager {
 public:
  FileManager() = default;

  ~FileManager() {
    for (const auto& file : unique_files_) {
      std::filesystem::remove(file);
    }
  }
  std::filesystem::path unique_file(const std::filesystem::path& location, bool keep = false) {
    auto dir = !location.empty() ? location : std::filesystem::temp_directory_path();

    auto file_name = dir / non_repeating_string_generator_.generate();
    while (std::filesystem::exists(file_name)) {
      file_name = dir / non_repeating_string_generator_.generate();
    }
    if (!keep)
      unique_files_.push_back(file_name);
    return file_name;
  }

  std::filesystem::path unique_file(bool keep = false) {
    return unique_file(std::string{}, keep);
  }


 protected:
  utils::NonRepeatingStringGenerator non_repeating_string_generator_;

  std::vector<std::filesystem::path> unique_files_;
};

}  // namespace org::apache::nifi::minifi::utils::file
