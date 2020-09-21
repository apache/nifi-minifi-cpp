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

#include "ConfigFile.h"

#include <fstream>

#include "utils/StringUtils.h"

namespace {
constexpr std::array<const char*, 2> DEFAULT_SENSITIVE_PROPERTIES{"nifi.security.client.pass.phrase",
                                                                  "nifi.rest.api.password"};
constexpr const char* ADDITIONAL_SENSITIVE_PROPS_PROPERTY_NAME = "nifi.sensitive.props.additional.keys";
}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace encrypt_config {

ConfigLine::ConfigLine(std::string line) : line_(line) {
  line = utils::StringUtils::trim(line);
  if (line.empty() || line[0] == '#') { return; }

  size_t index_of_first_equals_sign = line.find('=');
  if (index_of_first_equals_sign == std::string::npos) { return; }

  std::string key = utils::StringUtils::trim(line.substr(0, index_of_first_equals_sign));
  if (key.empty()) { return; }

  key_ = key;
  value_ = utils::StringUtils::trim(line.substr(index_of_first_equals_sign + 1));
}

ConfigLine::ConfigLine(const std::string& key, const std::string& value)
  : line_{key + "=" + value}, key_{key}, value_{value} {
}

void ConfigLine::updateValue(const std::string& value) {
  auto pos = line_.find('=');
  if (pos != std::string::npos) {
    line_.replace(pos + 1, std::string::npos, value);
    value_ = value;
  } else {
    throw std::invalid_argument{"Cannot update value in config line: it does not contain an = sign!"};
  }
}

ConfigFile::ConfigFile(const std::string& file_path) {
  std::ifstream file{file_path};
  std::string line;
  while (std::getline(file, line)) {
    config_lines_.push_back(ConfigLine{line});
  }
}

ConfigFile::Lines::const_iterator ConfigFile::findKey(const std::string& key) const {
  return std::find_if(config_lines_.cbegin(), config_lines_.cend(), [&key](const ConfigLine& config_line) {
    return config_line.getKey() == key;
  });
}

ConfigFile::Lines::iterator ConfigFile::findKey(const std::string& key) {
  return std::find_if(config_lines_.begin(), config_lines_.end(), [&key](const ConfigLine& config_line) {
    return config_line.getKey() == key;
  });
}

utils::optional<std::string> ConfigFile::getValue(const std::string& key) const {
  const auto it = findKey(key);
  if (it != config_lines_.end()) {
    return it->getValue();
  } else {
    return utils::nullopt;
  }
}

void ConfigFile::update(const std::string& key, const std::string& value) {
  auto it = findKey(key);
  if (it != config_lines_.end()) {
    it->updateValue(value);
  } else {
    throw std::invalid_argument{"Key " + key + " not found in the config file!"};
  }
}

void ConfigFile::insertAfter(const std::string& after_key, const std::string& key, const std::string& value) {
  auto it = findKey(after_key);
  if (it != config_lines_.end()) {
    ++it;
    config_lines_.emplace(it, key, value);
  } else {
    throw std::invalid_argument{"Key " + after_key + " not found in the config file!"};
  }
}

void ConfigFile::append(const std::string& key, const std::string& value) {
  config_lines_.emplace_back(key, value);
}

int ConfigFile::erase(const std::string& key) {
  auto has_this_key = [&key](const ConfigLine& line) { return line.getKey() == key; };
  auto new_end = std::remove_if(config_lines_.begin(), config_lines_.end(), has_this_key);
  auto num_removed = std::distance(new_end, config_lines_.end());
  config_lines_.erase(new_end, config_lines_.end());
  return gsl::narrow<int>(num_removed);
}

void ConfigFile::writeTo(const std::string& file_path) const {
  std::ofstream file{file_path};
  for (const auto& config_line : config_lines_) {
    file << config_line.getLine() << '\n';
  }
}

std::vector<std::string> ConfigFile::getSensitiveProperties() const {
  std::vector<std::string> sensitive_properties(DEFAULT_SENSITIVE_PROPERTIES.begin(), DEFAULT_SENSITIVE_PROPERTIES.end());
  const utils::optional<std::string> additional_sensitive_props_list = getValue(ADDITIONAL_SENSITIVE_PROPS_PROPERTY_NAME);
  if (additional_sensitive_props_list) {
    std::vector<std::string> additional_sensitive_properties = utils::StringUtils::split(*additional_sensitive_props_list, ",");
    sensitive_properties = mergeProperties(sensitive_properties, additional_sensitive_properties);
  }

  const auto not_found = [this](const std::string& property_name) { return !getValue(property_name); };
  const auto new_end = std::remove_if(sensitive_properties.begin(), sensitive_properties.end(), not_found);
  sensitive_properties.erase(new_end, sensitive_properties.end());

  return sensitive_properties;
}

std::vector<std::string> ConfigFile::mergeProperties(std::vector<std::string> properties,
                                                     const std::vector<std::string>& additional_properties) {
  for (const auto& property_name : additional_properties) {
    std::string property_name_trimmed = utils::StringUtils::trim(property_name);
    if (!property_name_trimmed.empty()) {
      properties.push_back(std::move(property_name_trimmed));
    }
  }

  std::sort(properties.begin(), properties.end());
  auto new_end = std::unique(properties.begin(), properties.end());
  properties.erase(new_end, properties.end());
  return properties;
}

}  // namespace encrypt_config
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
