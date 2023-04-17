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
#include "properties/Properties.h"
#include <fstream>
#include <string>
#include <utility>
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "core/logging/LoggerConfiguration.h"
#include "properties/PropertiesFile.h"
#include "properties/Configuration.h"

namespace org::apache::nifi::minifi {

Properties::Properties(std::string name)
    : logger_(core::logging::LoggerFactory<Properties>::getLogger()),
    name_(std::move(name)) {
}

// Get the config value
bool Properties::getString(const std::string &key, std::string &value) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

  if (it != properties_.end()) {
    value = it->second.active_value;
    return true;
  } else {
    return false;
  }
}

std::optional<std::string> Properties::getString(const std::string& key) const {
  std::string result;
  const bool found = getString(key, result);
  if (found) {
    return result;
  } else {
    return std::nullopt;
  }
}

int Properties::getInt(const std::string &key, int default_value) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

  return it != properties_.end() ? std::stoi(it->second.active_value) : default_value;
}

namespace {
const core::PropertyValidator* getValidator(const std::string& lookup_value) {
  auto configuration_property = Configuration::CONFIGURATION_PROPERTIES.find(lookup_value);

  if (configuration_property != Configuration::CONFIGURATION_PROPERTIES.end())
    return configuration_property->second;
  return nullptr;
}

std::optional<std::string> ensureTimePeriodValidatedPropertyHasExplicitUnit(const core::PropertyValidator* const validator, std::string& value) {
  if (validator != &core::StandardValidators::TIME_PERIOD_VALIDATOR)
    return std::nullopt;
  if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c){ return ::isdigit(c); }))
    return std::nullopt;

  return value + " ms";
}

std::optional<std::string> ensureDataSizeValidatedPropertyHasExplicitUnit(const core::PropertyValidator* const validator, std::string& value) {
  if (validator != &core::StandardValidators::DATA_SIZE_VALIDATOR)
    return std::nullopt;
  if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c){ return ::isdigit(c); }))
    return std::nullopt;

  return value + " B";
}

bool integerValidatedProperty(const core::PropertyValidator* const validator) {
  return validator == &core::StandardValidators::INTEGER_VALIDATOR
      || validator == &core::StandardValidators::UNSIGNED_INT_VALIDATOR
      || validator == &core::StandardValidators::LONG_VALIDATOR
      || validator == &core::StandardValidators::UNSIGNED_LONG_VALIDATOR;
}

std::optional<int64_t> stringToDataSize(std::string_view input) {
  int64_t value;
  std::string unit_str;
  if (!utils::StringUtils::splitToValueAndUnit(input, value, unit_str)) {
    return std::nullopt;
  }

  if (auto unit_multiplier = core::DataSizeValue::getUnitMultiplier(unit_str)) {
    return value * *unit_multiplier;
  }
  return std::nullopt;
}

std::optional<std::string> ensureIntegerValidatedPropertyHasNoUnit(const core::PropertyValidator* const validator, std::string& value) {
  if (!integerValidatedProperty(validator)) {
    return std::nullopt;
  }

  if (auto parsed_time = utils::timeutils::StringToDuration<std::chrono::milliseconds>(value)) {
    return fmt::format("{}", parsed_time->count());
  }

  if (auto parsed_data_size = stringToDataSize(value)) {
    return fmt::format("{}", *parsed_data_size);
  }

  return std::nullopt;
}

void fixValidatedProperty(const std::string& property_name,
    std::string& persisted_value,
    std::string& value,
    bool& needs_to_persist_new_value,
    core::logging::Logger& logger) {
  auto validator = getValidator(property_name);
  if (!validator)
    return;

  auto fixed_property_value = ensureTimePeriodValidatedPropertyHasExplicitUnit(validator, value)
      | utils::valueOrElse([&] { return ensureDataSizeValidatedPropertyHasExplicitUnit(validator, value);})
      | utils::valueOrElse([&] { return ensureIntegerValidatedPropertyHasNoUnit(validator, value);});

  if (!fixed_property_value) {
    return;
  }

  if (persisted_value == value) {
    logger.log_info("Changed validated property from %s to %s, this change will be persisted",  value, *fixed_property_value);
    value = *fixed_property_value;
    persisted_value = value;
    needs_to_persist_new_value = true;
  } else {
    logger.log_info("Changed validated property from %s to %s, this change won't be persisted", value, *fixed_property_value);
    value = *fixed_property_value;
    needs_to_persist_new_value = false;
  }
}
}  // namespace

// Load Configure File
// If the loaded property is time-period or data-size validated and it has no explicit units ms or B will be appended.
// If the loaded property is integer validated and it has some explicit unit(time-period or data-size) it will be converted to ms/B and its unit cut off
void Properties::loadConfigureFile(const std::filesystem::path& configuration_file, std::string_view prefix) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (configuration_file.empty()) {
    logger_->log_error("Configuration file path for %s is empty!", getName());
    return;
  }

  std::error_code ec;
  properties_file_ = std::filesystem::canonical(getHome() / configuration_file, ec);

  if (ec.value() != 0) {
    logger_->log_warn("Configuration file '%s' does not exist, so it could not be loaded.", configuration_file.string());
    return;
  }

  logger_->log_info("Using configuration file to load configuration for %s from %s (located at %s)",
                    getName().c_str(), configuration_file.string(), properties_file_.string());

  std::ifstream file(properties_file_, std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load configure file failed %s", properties_file_.string());
    return;
  }
  properties_.clear();
  dirty_ = false;
  for (const auto& line : PropertiesFile{file}) {
    auto key = line.getKey();
    auto persisted_value = line.getValue();
    auto value = utils::StringUtils::replaceEnvironmentVariables(persisted_value);
    bool need_to_persist_new_value = false;
    fixValidatedProperty(std::string(prefix) + key, persisted_value, value, need_to_persist_new_value, *logger_);
    dirty_ = dirty_ || need_to_persist_new_value;
    properties_[key] = {persisted_value, value, need_to_persist_new_value};
  }
  checksum_calculator_.setFileLocation(properties_file_);
}

std::filesystem::path Properties::getFilePath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return properties_file_;
}

bool Properties::commitChanges() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!dirty_) {
    logger_->log_info("Attempt to persist, but properties are not updated");
    return true;
  }
  std::ifstream file(properties_file_, std::ifstream::in);
  if (!file) {
    logger_->log_error("load configure file failed %s", properties_file_.string());
    return false;
  }

  auto new_file = properties_file_;
  new_file += ".new";

  PropertiesFile current_content{file};
  for (const auto& prop : properties_) {
    if (!prop.second.need_to_persist_new_value) {
      continue;
    }
    if (current_content.hasValue(prop.first)) {
      current_content.update(prop.first, prop.second.persisted_value);
    } else {
      current_content.append(prop.first, prop.second.persisted_value);
    }
  }

  try {
    current_content.writeTo(new_file);
  } catch (const std::exception&) {
    logger_->log_error("Could not update %s", properties_file_.string());
    return false;
  }

  auto backup = properties_file_;
  backup += ".bak";
  if (utils::file::FileUtils::copy_file(properties_file_, backup) == 0 && utils::file::FileUtils::copy_file(new_file, properties_file_) == 0) {
    logger_->log_info("Persisted %s", properties_file_.string());
    checksum_calculator_.invalidateChecksum();
    dirty_ = false;
    return true;
  }

  logger_->log_error("Could not update %s", properties_file_.string());
  return false;
}

std::map<std::string, std::string> Properties::getProperties() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::map<std::string, std::string> properties;
  for (const auto& prop : properties_) {
    properties[prop.first] = prop.second.active_value;
  }
  return properties;
}

}  // namespace org::apache::nifi::minifi
