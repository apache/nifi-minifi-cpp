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

#include "properties/PropertiesFile.h"

#include <algorithm>
#include <fstream>
#include <utility>
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

PropertiesFile::Line::Line(std::string line) : line_(line) {
  line = utils::StringUtils::trim(line);
  if (line.empty() || line[0] == '#') { return; }

  size_t index_of_first_equals_sign = line.find('=');
  if (index_of_first_equals_sign == std::string::npos) { return; }

  std::string key = utils::StringUtils::trim(line.substr(0, index_of_first_equals_sign));
  if (key.empty()) { return; }

  key_ = key;
  value_ = utils::StringUtils::trim(line.substr(index_of_first_equals_sign + 1));
}

PropertiesFile::Line::Line(std::string key, std::string value)
  : line_{utils::StringUtils::join_pack(key, "=", value)}, key_{std::move(key)}, value_{std::move(value)} {
}

bool PropertiesFile::Line::isValidKey(const std::string &key) {
  return !key.empty();
}

void PropertiesFile::Line::updateValue(const std::string& value) {
  auto pos = line_.find('=');
  if (pos != std::string::npos) {
    line_.replace(pos + 1, std::string::npos, value);
    value_ = value;
  } else {
    throw std::invalid_argument{"Cannot update value in config line: it does not contain an = sign!"};
  }
}

PropertiesFile::PropertiesFile(std::istream& input_stream) {
  std::string line;
  while (std::getline(input_stream, line)) {
    lines_.push_back(Line{line});
  }
}

PropertiesFile::Lines::const_iterator PropertiesFile::findKey(const std::string& key) const {
  if (!Line::isValidKey(key)) {
    return lines_.cend();
  }
  return std::find_if(lines_.cbegin(), lines_.cend(), [&key](const Line& line) {
    return line.getKey() == key;
  });
}

PropertiesFile::Lines::iterator PropertiesFile::findKey(const std::string& key) {
  if (!Line::isValidKey(key)) {
    return lines_.end();
  }
  return std::find_if(lines_.begin(), lines_.end(), [&key](const Line& line) {
    return line.getKey() == key;
  });
}

bool PropertiesFile::hasValue(const std::string& key) const {
  return findKey(key) != lines_.end();
}

std::optional<std::string> PropertiesFile::getValue(const std::string& key) const {
  const auto it = findKey(key);
  if (it != lines_.end()) {
    return it->getValue();
  } else {
    return std::nullopt;
  }
}

void PropertiesFile::update(const std::string& key, const std::string& value) {
  auto it = findKey(key);
  if (it != lines_.end()) {
    it->updateValue(value);
  } else {
    throw std::invalid_argument{"Key " + key + " not found in the config file!"};
  }
}

void PropertiesFile::insertAfter(const std::string& after_key, const std::string& key, const std::string& value) {
  auto it = findKey(after_key);
  if (it != lines_.end()) {
    ++it;
    lines_.emplace(it, key, value);
  } else {
    throw std::invalid_argument{"Key " + after_key + " not found in the config file!"};
  }
}

void PropertiesFile::append(const std::string& key, const std::string& value) {
  lines_.emplace_back(key, value);
}

int PropertiesFile::erase(const std::string& key) {
  if (!Line::isValidKey(key)) {
    return 0;
  }
  auto has_this_key = [&key](const Line& line) { return line.getKey() == key; };
  auto new_end = std::remove_if(lines_.begin(), lines_.end(), has_this_key);
  auto num_removed = std::distance(new_end, lines_.end());
  lines_.erase(new_end, lines_.end());
  return gsl::narrow<int>(num_removed);
}

void PropertiesFile::writeTo(const std::string& file_path) const {
  try {
    std::ofstream file{file_path};
    file.exceptions(std::ios::failbit | std::ios::badbit);

    for (const auto& line : lines_) {
      file << line.getLine() << '\n';
    }
  } catch (const std::exception&) {
    throw std::runtime_error{"Could not write to file " + file_path};
  }
}

PropertiesFile::Lines::const_iterator PropertiesFile::begin() const {
  return lines_.begin();
}

PropertiesFile::Lines::const_iterator PropertiesFile::end() const {
  return lines_.end();
}

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
