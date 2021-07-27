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

#include <istream>
#include <optional>
#include <string>
#include <vector>

#include "utils/crypto/EncryptionUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class PropertiesFile {
 public:
  class Line {
   public:
    explicit Line(std::string line);
    Line(std::string key, std::string value);

    void updateValue(const std::string& value);

    [[nodiscard]] std::string getLine() const { return line_; }
    [[nodiscard]] std::string getKey() const { return key_; }
    [[nodiscard]] std::string getValue() const { return value_; }

    static bool isValidKey(const std::string& key);

   private:
    friend bool operator==(const Line&, const Line&);
    // NOTE(fgerlits): having both line_ and { key_, value } is redundant in many cases, but
    // * we need the original line_ in order to preserve formatting, comments and blank lines
    // * we could get rid of key_ and value_ and parse them each time from line_, but I think the code is clearer this way
    std::string line_;
    std::string key_;
    std::string value_;
  };

  explicit PropertiesFile(std::istream& input_stream);
  explicit PropertiesFile(std::istream&& input_stream) : PropertiesFile{input_stream} {}

  [[nodiscard]] bool hasValue(const std::string& key) const;
  [[nodiscard]] std::optional<std::string> getValue(const std::string& key) const;
  void update(const std::string& key, const std::string& value);
  void insertAfter(const std::string& after_key, const std::string& key, const std::string& value);
  void append(const std::string& key, const std::string& value);
  int erase(const std::string& key);

  void writeTo(const std::string& file_path) const;

  [[nodiscard]] size_t size() const { return lines_.size(); }

 protected:
  using Lines = std::vector<Line>;

  [[nodiscard]] Lines::const_iterator findKey(const std::string& key) const;
  Lines::iterator findKey(const std::string& key);

 public:
  [[nodiscard]] Lines::const_iterator begin() const;
  [[nodiscard]] Lines::const_iterator end() const;

 protected:
  Lines lines_;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
