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

#include "core/logging/LoggerConfiguration.h"
#include "properties/Configuration.h"
#include "properties/PropertiesFile.h"
#include "range/v3/algorithm/all_of.hpp"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi {

PropertiesImpl::PropertiesImpl(std::string name)
    : logger_(core::logging::LoggerFactory<Properties>::getLogger()),
    name_(std::move(name)) {
}

// Get the config value
bool PropertiesImpl::getString(const std::string &key, std::string &value) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = properties_.find(key);

  if (it != properties_.end()) {
    value = it->second.active_value;
    return true;
  } else {
    return false;
  }
}

std::optional<std::string> PropertiesImpl::getString(const std::string& key) const {
  if (std::string result; getString(key, result)) {
    return result;
  }
  return std::nullopt;
}

int PropertiesImpl::getInt(const std::string &key, int default_value) const {
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

// isdigit requires unsigned chars as input
bool allDigits(const std::string& value) {
  return ranges::all_of(value, [](const unsigned char c){ return ::isdigit(c); });
}

// isdigit requires unsigned chars as input
bool allDigitsOrSpaces(const std::string& value) {
  return ranges::all_of(value, [](const unsigned char c) { return std::isdigit(c) || std::isspace(c);});
}

std::optional<std::string> ensureTimePeriodValidatedPropertyHasExplicitUnit(const core::PropertyValidator* const validator, const std::string& value) {
  if (validator != &core::StandardPropertyTypes::TIME_PERIOD_TYPE) {
    return std::nullopt;
  }
  if (value.empty() || !allDigits(value)) {
    return std::nullopt;
  }

  return value + " ms";
}

std::optional<std::string> ensureDataSizeValidatedPropertyHasExplicitUnit(const core::PropertyValidator* const validator, const std::string& value) {
  if (validator != &core::StandardPropertyTypes::DATA_SIZE_TYPE) {
    return std::nullopt;
  }
  if (value.empty() || !allDigits(value)) {
    return std::nullopt;
  }

  return value + " B";
}

bool integerValidatedProperty(const core::PropertyValidator* const validator) {
  return validator == &core::StandardPropertyTypes::INTEGER_TYPE
      || validator == &core::StandardPropertyTypes::UNSIGNED_INT_TYPE
      || validator == &core::StandardPropertyTypes::LONG_TYPE
      || validator == &core::StandardPropertyTypes::UNSIGNED_LONG_TYPE;
}

std::optional<int64_t> stringToDataSize(const std::string_view input) {
  int64_t value = 0;
  std::string unit_str;
  if (!utils::string::splitToValueAndUnit(input, value, unit_str)) {
    return std::nullopt;
  }

  if (const auto unit_multiplier = core::DataSizeValue::getUnitMultiplier(unit_str)) {
    return value * *unit_multiplier;
  }
  return std::nullopt;
}

std::optional<int64_t> stringToDataTransferSpeed(std::string_view input) {
  std::string data_size;
  try {
    data_size = core::DataTransferSpeedValue::removePerSecSuffix(std::string(input));
  } catch (const utils::internal::ParseException&) {
    return std::nullopt;
  }
  return stringToDataSize(data_size);
}

std::optional<std::string> ensureIntegerValidatedPropertyHasNoUnit(const core::PropertyValidator* const validator, const std::string& value) {
  if (!integerValidatedProperty(validator)) {
    return std::nullopt;
  }

  if (allDigitsOrSpaces(value)) {
    return std::nullopt;
  }

  if (const auto parsed_time = utils::timeutils::StringToDuration<std::chrono::milliseconds>(value)) {
    return fmt::format("{}", parsed_time->count());
  }

  if (auto parsed_data_size = stringToDataSize(value)) {
    return fmt::format("{}", *parsed_data_size);
  }

  if (auto parsed_data_transfer_speed = stringToDataTransferSpeed(value)) {
    return fmt::format("{}", *parsed_data_transfer_speed);
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
    logger.log_info("Changed validated property from {} to {}, this change will be persisted",  value, *fixed_property_value);
    value = *fixed_property_value;
    persisted_value = value;
    needs_to_persist_new_value = true;
  } else {
    logger.log_info("Changed validated property from {} to {}, this change won't be persisted", value, *fixed_property_value);
    value = *fixed_property_value;
    needs_to_persist_new_value = false;
  }
}
}  // namespace

// Load Configure File
// If the loaded property is time-period or data-size validated and it has no explicit units ms or B will be appended.
// If the loaded property is integer validated and it has some explicit unit(time-period or data-size) it will be converted to ms/B and its unit cut off
void PropertiesImpl::loadConfigureFile(const std::filesystem::path& configuration_file, std::string_view prefix) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (configuration_file.empty()) {
    logger_->log_error("Configuration file path for {} is empty!", getName());
    return;
  }

  if (!utils::file::exists(getHome() / configuration_file)) {
    if (utils::file::create_dir((getHome() / configuration_file).parent_path()) == 0) {
      std::ofstream file{getHome() / configuration_file};
    }
  }

  std::error_code ec;
  properties_file_ = std::filesystem::canonical(getHome() / configuration_file, ec);

  if (ec.value() != 0) {
    logger_->log_warn("Configuration file '{}' does not exist, and it could not be created", configuration_file);
    return;
  }

  logger_->log_info("Using configuration file to load configuration for {} from {} (located at {})",
                    getName().c_str(), configuration_file.string(), properties_file_.string());

  std::ifstream file(properties_file_, std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load configure file failed {}", properties_file_);
    return;
  }
  properties_.clear();
  dirty_ = false;
  for (const auto& line : PropertiesFile{file}) {
    auto key = line.getKey();
    auto persisted_value = line.getValue();
    auto value = utils::string::replaceEnvironmentVariables(persisted_value);
    bool need_to_persist_new_value = false;
    fixValidatedProperty(std::string(prefix) + key, persisted_value, value, need_to_persist_new_value, *logger_);
    dirty_ = dirty_ || need_to_persist_new_value;
    properties_[key] = {persisted_value, value, need_to_persist_new_value};
  }
  checksum_calculator_.setFileLocation(properties_file_);
}

std::filesystem::path PropertiesImpl::getFilePath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return properties_file_;
}

bool PropertiesImpl::commitChanges() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!dirty_) {
    logger_->log_info("Attempt to persist, but properties are not updated");
    return true;
  }
  std::ifstream file(properties_file_, std::ifstream::in);
  if (!file) {
    logger_->log_error("load configure file failed {}", properties_file_);
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
    logger_->log_error("Could not update {}", properties_file_);
    return false;
  }

  auto backup = properties_file_;
  backup += ".bak";
  if (utils::file::FileUtils::copy_file(properties_file_, backup) == 0 && utils::file::FileUtils::copy_file(new_file, properties_file_) == 0) {
    logger_->log_info("Persisted {}", properties_file_);
    checksum_calculator_.invalidateChecksum();
    dirty_ = false;
    return true;
  }

  logger_->log_error("Could not update {}", properties_file_);
  return false;
}

std::map<std::string, std::string> PropertiesImpl::getProperties() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::map<std::string, std::string> properties;
  for (const auto& prop : properties_) {
    properties[prop.first] = prop.second.active_value;
  }
  return properties;
}

std::shared_ptr<Properties> Properties::create() {
  return std::make_shared<PropertiesImpl>();
}

}  // namespace org::apache::nifi::minifi
