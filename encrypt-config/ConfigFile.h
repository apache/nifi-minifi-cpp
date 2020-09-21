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

#include "utils/EncryptionUtils.h"
#include "utils/OptionalUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

class ConfigLine {
 public:
  ConfigLine(std::string line);
  ConfigLine(const std::string& key, const std::string& value);

  void updateValue(const std::string& value);

  std::string getLine() const { return line_; }
  std::string getKey() const { return key_; }
  std::string getValue() const { return value_; }

 private:
  // NOTE(fgerlits): having both line_ and { key_, value } is redundant in many cases, but
  // * we need the original line_ in order to preserve formatting, comments and blank lines
  // * we could get rid of key_ and value_ and parse them each time from line_, but I think the code is clearer this way
  std::string line_;
  std::string key_;
  std::string value_;
};

inline bool operator==(const ConfigLine& left, const ConfigLine& right) {
  return left.getLine() == right.getLine();
}

class ConfigFile {
public:
  ConfigFile(const std::string& file_path);

  utils::optional<std::string> getValue(const std::string& key) const;
  void update(const std::string& key, const std::string& value);
  void insertAfter(const std::string& after_key, const std::string& key, const std::string& value);
  void append(const std::string& key, const std::string& value);
  int erase(const std::string& key);

  void writeTo(const std::string& file_path) const;

  size_t size() const { return config_lines_.size(); }

  int encryptSensitiveProperties(const utils::crypto::Bytes& encryption_key);

private:
  friend class ConfigFileTestAccessor;
  friend bool operator==(const ConfigFile&, const ConfigFile&);
  using Lines = std::vector<ConfigLine>;

  Lines::const_iterator findKey(const std::string& key) const;
  Lines::iterator findKey(const std::string& key);
  std::vector<std::string> getSensitiveProperties() const;
  static std::vector<std::string> mergeProperties(std::vector<std::string> properties,
                                                  const std::vector<std::string>& additional_properties);

  Lines config_lines_;
};

inline bool operator==(const ConfigFile& left, const ConfigFile& right) {
  return left.config_lines_ == right.config_lines_;
}

}  // namespace encrypt_config
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
