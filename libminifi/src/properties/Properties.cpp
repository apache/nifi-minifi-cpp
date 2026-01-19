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
#include <ranges>
#include <string>

#include "core/logging/LoggerConfiguration.h"
#include "properties/Configuration.h"
#include "properties/PropertiesFile.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi {

PropertiesImpl::PropertiesImpl(PersistTo persist_to, std::string name)
    : logger_(core::logging::LoggerFactory<Properties>::getLogger()),
    persist_to_(persist_to),
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
  const auto configuration_property = Configuration::CONFIGURATION_PROPERTIES.find(lookup_value);

  if (configuration_property != Configuration::CONFIGURATION_PROPERTIES.end())
    return configuration_property->second;
  return nullptr;
}

// isdigit requires unsigned chars as input
bool allDigits(const std::string& value) {
  return std::ranges::all_of(value, [](const unsigned char c){ return ::isdigit(c); });
}

// isdigit requires unsigned chars as input
bool allDigitsOrSpaces(const std::string& value) {
  return std::ranges::all_of(value, [](const unsigned char c) { return std::isdigit(c) || std::isspace(c);});
}

std::optional<std::string> ensureTimePeriodValidatedPropertyHasExplicitUnit(const core::PropertyValidator* const validator, const std::string& value) {
  if (validator != &core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR) {
    return std::nullopt;
  }
  if (value.empty() || !allDigits(value)) {
    return std::nullopt;
  }

  return value + " ms";
}

std::optional<std::string> ensureDataSizeValidatedPropertyHasExplicitUnit(const core::PropertyValidator* const validator, const std::string& value) {
  if (validator != &core::StandardPropertyValidators::DATA_SIZE_VALIDATOR) {
    return std::nullopt;
  }
  if (value.empty() || !allDigits(value)) {
    return std::nullopt;
  }

  return value + " B";
}

bool integerValidatedProperty(const core::PropertyValidator* const validator) {
  return validator == &core::StandardPropertyValidators::INTEGER_VALIDATOR
      || validator == &core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR;
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

  if (auto parsed_data_size = parsing::parseDataSize(value)) {
    return fmt::format("{}", *parsed_data_size);
  }

  return std::nullopt;
}

// If the loaded property is time period or data size validated, and it has no explicit units, then ms or B will be appended.
// If the loaded property is integer validated, and it has some explicit unit (time period or data size), it will be converted to ms or B, and its unit is cut off.
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

auto getExtraPropertiesFileNames(const std::filesystem::path& extra_properties_files_dir, const std::shared_ptr<core::logging::Logger>& logger) {
  std::vector<std::filesystem::path> extra_properties_file_names;
  if (utils::file::exists(extra_properties_files_dir) && utils::file::is_directory(extra_properties_files_dir)) {
    utils::file::list_dir(extra_properties_files_dir, [&](const std::filesystem::path&, const std::filesystem::path& file_name) {
      if (file_name.string().ends_with(".properties")) {
        extra_properties_file_names.push_back(file_name);
      }
      return true;
    }, logger, /* recursive = */ false);
  }
  std::ranges::sort(extra_properties_file_names);
  return extra_properties_file_names;
}

void updateChangedPropertiesInPropertiesFile(minifi::PropertiesFile& current_content, const auto& properties) {
  for (const auto& prop : properties) {
    if (!prop.second.need_to_persist_new_value) {
      continue;
    }
    if (current_content.hasValue(prop.first)) {
      current_content.update(prop.first, prop.second.persisted_value);
    } else {
      current_content.append(prop.first, prop.second.persisted_value);
    }
  }
}
}  // namespace

std::filesystem::path PropertiesImpl::extraPropertiesFilesDirName() const {
  auto extra_properties_files_dir = base_properties_file_;
  extra_properties_files_dir += ".d";
  return extra_properties_files_dir;
}

void PropertiesImpl::loadConfigureFile(const std::filesystem::path& configuration_file, std::string_view prefix) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (configuration_file.empty()) {
    logger_->log_error("Configuration file path for {} is empty!", getName());
    return;
  }

  if (!utils::file::exists(configuration_file)) {
    if (utils::file::create_dir(configuration_file.parent_path()) == 0) {
      std::ofstream file{configuration_file};
    }
  }

  std::error_code ec;
  base_properties_file_ = std::filesystem::canonical(configuration_file, ec);

  if (ec.value() != 0) {
    logger_->log_warn("Configuration file '{}' does not exist, and it could not be created", configuration_file);
    return;
  }

  properties_files_ = { base_properties_file_ };
  const auto extra_properties_files_dir = extraPropertiesFilesDirName();
  const auto extra_properties_file_names = getExtraPropertiesFileNames(extra_properties_files_dir, logger_);
  for (const auto& file_name : extra_properties_file_names) {
    properties_files_.push_back(extra_properties_files_dir / file_name);
  }

  logger_->log_info("Using configuration file to load configuration for {} from {} (located at {})",
                    getName().c_str(), configuration_file.string(), base_properties_file_.string());
  if (!extra_properties_file_names.empty()) {
    auto list_of_files = utils::string::join(", ", extra_properties_file_names, [](const auto& path) { return path.string(); });
    logger_->log_info("Also reading configuration from files {} in {}", list_of_files, extra_properties_files_dir.string());
  }

  properties_.clear();
  dirty_ = false;
  for (const auto& properties_file : properties_files_) {
    setPropertiesFromFile(properties_file, prefix);
  }

  checksum_calculator_.setFileLocations(properties_files_);
}

void PropertiesImpl::setPropertiesFromFile(const std::filesystem::path& properties_file, std::string_view prefix) {
  std::ifstream file(properties_file, std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load configure file failed {}", properties_file);
    return;
  }
  for (const auto& line : PropertiesFile{file}) {
    auto key = line.getKey();
    auto persisted_value = line.getValue();
    auto value = utils::string::replaceEnvironmentVariables(persisted_value);
    bool need_to_persist_new_value = false;
    fixValidatedProperty(std::string(prefix) + key, persisted_value, value, need_to_persist_new_value, *logger_);
    dirty_ = dirty_ || need_to_persist_new_value;
    properties_[key] = {persisted_value, value, need_to_persist_new_value};
  }
}

std::filesystem::path PropertiesImpl::getFilePath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return base_properties_file_;
}

bool PropertiesImpl::commitChanges() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!dirty_) {
    logger_->log_debug("commitChanges() called, but properties have not changed, nothing to do");
    return true;
  }
  const auto output_file = (persist_to_ == PersistTo::SingleFile ? base_properties_file_ : extraPropertiesFilesDirName() / C2PropertiesFileName);
  if (!std::filesystem::exists(output_file)) {
    logger_->log_debug("Configuration file {} does not exist yet, creating it", output_file);
    utils::file::create_dir(output_file.parent_path(), /* recursive = */ true);
    std::ofstream file{output_file};
  }

  std::ifstream file(output_file, std::ifstream::in);
  if (!file) {
    logger_->log_error("Failed to load configuration file {}", output_file);
    return false;
  }
  PropertiesFile current_content{file};
  file.close();

  updateChangedPropertiesInPropertiesFile(current_content, properties_);

  auto new_file = output_file;
  new_file += ".new";
  try {
    current_content.writeTo(new_file);
  } catch (const std::exception&) {
    logger_->log_error("Could not write to {}", new_file);
    return false;
  }

  std::error_code ec;
  const auto existing_file_size = std::filesystem::file_size(output_file, ec);
  if (ec || existing_file_size == 0) {
    if (!utils::file::move_file(new_file, output_file)) {
      logger_->log_error("Could not create minifi properties file {}", output_file);
      return false;
    }
  } else {
    auto backup = output_file;
    backup += ".bak";
    if (!utils::file::move_file(output_file, backup) || !utils::file::move_file(new_file, output_file)) {
      logger_->log_error("Could not update minifi properties file {}", output_file);
      return false;
    }
  }

  logger_->log_info("Persisted {}", output_file);
  checksum_calculator_.invalidateChecksum();
  dirty_ = false;
  return true;
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
  return std::make_shared<PropertiesImpl>(PropertiesImpl::PersistTo::SingleFile);
}

}  // namespace org::apache::nifi::minifi
