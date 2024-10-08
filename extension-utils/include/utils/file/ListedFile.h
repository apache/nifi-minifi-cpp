/**
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

#include <utility>
#include <string>

#include "../ListingStateManager.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::utils {

struct FileFilter {
  std::optional<std::regex> filename_filter;
  std::optional<std::regex> path_filter;
  std::optional<std::chrono::milliseconds> minimum_file_age;
  std::optional<std::chrono::milliseconds> maximum_file_age;
  std::optional<uint64_t> minimum_file_size;
  std::optional<uint64_t> maximum_file_size;
  bool ignore_hidden_files = true;
};

class ListedFile : public utils::ListedObject {
 public:
  explicit ListedFile(std::filesystem::path full_file_path, std::filesystem::path input_directory) : full_file_path_(std::move(full_file_path)), input_directory_(std::move(input_directory)) {
    if (auto last_write_time = utils::file::last_write_time(full_file_path_)) {
      last_modified_time_ = utils::file::to_sys(*last_write_time);
    }
  }

  [[nodiscard]] std::chrono::system_clock::time_point getLastModified() const override {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(last_modified_time_);
  }

  [[nodiscard]] std::string getKey() const override {
    return full_file_path_.string();
  }

  [[nodiscard]] const std::filesystem::path& getPath() const {
    return full_file_path_;
  }

  [[nodiscard]] const std::filesystem::path& getDirectory() const {
    return input_directory_;
  }

  [[nodiscard]] bool matches(const FileFilter& file_filter) {
    if (file_filter.ignore_hidden_files && utils::file::FileUtils::is_hidden(full_file_path_))
      return false;

    return fileAgeIsBetween(file_filter.minimum_file_age, file_filter.maximum_file_age) &&
        fileSizeIsBetween(file_filter.minimum_file_size, file_filter.maximum_file_size) &&
        matchesRegex(file_filter.filename_filter, file_filter.path_filter);
  }

 private:
  [[nodiscard]] bool matchesRegex(const std::optional<std::regex>& file_regex, const std::optional<std::regex>& path_regex) const {
    if (file_regex && !std::regex_match(full_file_path_.filename().string(), *file_regex))
      return false;
    if (path_regex && !std::regex_match(std::filesystem::relative(full_file_path_.parent_path(), input_directory_).string(), *path_regex))
      return false;
    return true;
  }

  [[nodiscard]] bool fileAgeIsBetween(const std::optional<std::chrono::milliseconds> minimum_age, const std::optional<std::chrono::milliseconds>  maximum_age) const {
    auto file_age = getAge();
    if (minimum_age && minimum_age > file_age)
      return false;
    if (maximum_age && maximum_age < file_age)
      return false;
    return true;
  }

  [[nodiscard]] bool fileSizeIsBetween(const std::optional<size_t> minimum_size, const std::optional<size_t> maximum_size) const {
    if (minimum_size && minimum_size > getSize())
      return false;
    if (maximum_size && maximum_size < getSize())
      return false;
    return true;
  }

  [[nodiscard]] std::chrono::system_clock::duration getAge() const { return std::chrono::system_clock::now() - last_modified_time_;}
  [[nodiscard]] size_t getSize() const { return utils::file::file_size(full_file_path_); }
  std::chrono::system_clock::time_point last_modified_time_;
  std::filesystem::path full_file_path_;
  std::filesystem::path input_directory_;
};

}  // namespace org::apache::nifi::minifi::utils
