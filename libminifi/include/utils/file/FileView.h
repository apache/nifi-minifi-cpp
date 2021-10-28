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

#include <filesystem>
#include <fstream>
#include <string>

#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils::file {

class FileIterator {
  friend class FileView;
  FileIterator(std::ifstream* file, std::ifstream::off_type offset)
    : file_(file), offset_(offset) {}

 public:
  using difference_type = std::ifstream::off_type;
  using iterator_category = std::forward_iterator_tag;
  using value_type = char;
  using reference = value_type;
  using pointer = void;

  FileIterator& operator++() {
    ++offset_;
    return *this;
  }

  FileIterator operator++(int) {
    FileIterator copy = *this;
    ++offset_;
    return copy;
  }

  char operator*() const {
    file_->seekg(offset_);
    char ch;
    file_->get(ch);
    return ch;
  }

  bool operator==(const FileIterator& other) const {
    return file_ == other.file_ && offset_ == other.offset_;
  }

  bool operator!=(const FileIterator& other) const {
    return !(*this == other);
  }

  std::ifstream::off_type operator-(const FileIterator& other) const {
    return offset_ - other.offset_;
  }

 private:
  std::ifstream* file_;
  std::ifstream::off_type offset_;
};

class FileView {
 public:
  explicit FileView(const std::filesystem::path& file) {
    file_.open(file, std::ifstream::in | std::ifstream::binary);
    if (!file_.is_open()) {
      throw std::ios_base::failure("Couldn't open file '" + file.string() + "'");
    }
    file_.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file_.seekg(0, std::ios_base::end);
    file_size_ = file_.tellg();
  }

  FileIterator begin() {
    return FileIterator{&file_, 0};
  }

  FileIterator end() {
    return FileIterator{&file_, gsl::narrow<std::ifstream::off_type>(file_size_)};
  }

 private:
  std::ifstream file_;
  size_t file_size_;
};

}  // namespace org::apache::nifi::minifi::utils::file
