/**
 * @file Logger.cpp
 * Logger class implementation
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

#include "core/logging/LoggerConfiguration.h"

#include <sys/stat.h>
#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/Core.h"
#include "utils/StringUtils.h"
#include "utils/ClassUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/Environment.h"
#include "core/logging/internal/LogCompressorSink.h"
#include "core/logging/alert/AlertSink.h"
#include "minifi-cpp/utils/Literals.h"
#include "core/TypedValues.h"
#include "core/logging/Utils.h"
#include "controllers/SSLContextService.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/null_sink.h"
#include "spdlog/pattern_formatter.h"

#ifdef WIN32
#include "core/logging/WindowsEventLogSink.h"
#else
#include "spdlog/sinks/syslog_sink.h"
#endif

#ifdef WIN32
#include <direct.h>
#define _WINSOCKAPI_
#include <windows.h>
#include <tchar.h>
#endif

namespace org::apache::nifi::minifi::core::logging {

const char* LoggerConfiguration::spdlog_default_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v";

namespace internal {

void LoggerNamespace::forEachSink(const std::function<void(const std::shared_ptr<spdlog::sinks::sink>&)>& op) const {
  for (auto& sink : sinks) {
    op(sink);
  }
  for (auto& sink : exported_sinks) {
    op(sink);
  }
  for (auto& [name, child] : children) {
    child->forEachSink(op);
  }
}

}  // namespace internal

std::vector<std::string> LoggerProperties::get_keys_of_type(const std::string &type) const {
  std::vector<std::string> appenders;
  const std::string prefix = type + ".";
  for (const auto & [property_name, _property_value] : getProperties()) {
    if (property_name.starts_with(prefix) && property_name.find('.', prefix.length() + 1) == std::string::npos) {
      appenders.push_back(property_name);
    }
  }
  return appenders;
}

LoggerConfiguration::LoggerConfiguration()
    : root_namespace_(create_default_root()),
      formatter_(std::make_shared<spdlog::pattern_formatter>(spdlog_default_pattern)) {
  const std::lock_guard<std::mutex> lock(mutex_);
  controller_ = std::make_shared<LoggerControl>();
  logger_ = std::make_shared<LoggerImpl>(
      std::string(core::className<LoggerConfiguration>()),
      std::nullopt,
      controller_,
      get_logger(lock, root_namespace_, std::string(core::className<LoggerConfiguration>()), formatter_));
}

LoggerConfiguration& LoggerConfiguration::getConfiguration() {
  static LoggerConfiguration instance;
  return instance;
}

void LoggerConfiguration::initialize(const std::shared_ptr<LoggerProperties> &logger_properties) {
  const std::lock_guard<std::mutex> lock(mutex_);
  root_namespace_ = initialize_namespaces(logger_properties, logger_);
  alert_sinks_.clear();
  root_namespace_->forEachSink([&] (const std::shared_ptr<spdlog::sinks::sink>& sink) {
    if (auto alert_sink = std::dynamic_pointer_cast<AlertSink>(sink)) {
      alert_sinks_.insert(std::move(alert_sink));
    }
  });
  initializeCompression(lock, logger_properties);
  std::string spdlog_pattern;
  if (!logger_properties->getString("spdlog.pattern", spdlog_pattern)) {
    spdlog_pattern = spdlog_default_pattern;
  }

  /**
   * There is no need to shorten names per spdlog sink as this is a per log instance.
   */
  if (const auto shorten_names_str = logger_properties->getString("spdlog.shorten_names")) {
    shorten_names_ = utils::string::toBool(*shorten_names_str).value_or(false);
  }

  if (const auto include_uuid_str = logger_properties->getString("logger.include.uuid")) {
    include_uuid_ = utils::string::toBool(*include_uuid_str).value_or(true);
  }

  if (const auto max_log_entry_length_str = logger_properties->getString("max.log.entry.length")) {
    try {
      if (internal::UNLIMITED_LOG_ENTRY_LENGTH == *max_log_entry_length_str) {
        max_log_entry_length_ = -1;
      } else {
        max_log_entry_length_ = std::stoi(*max_log_entry_length_str);
      }
    } catch (const std::exception& ex) {
      logger_->log_error("Parsing max log entry length property failed with the following exception: {}", ex.what());
    }
  }

  formatter_ = std::make_shared<spdlog::pattern_formatter>(spdlog_pattern);
  spdlog::apply_all([&](const auto& spd_logger) {
    setupSpdLogger(lock, spd_logger, root_namespace_, spd_logger->name(), formatter_);
  });
  logger_->log_debug("Set following pattern on loggers: {}", spdlog_pattern);
}

std::shared_ptr<Logger> LoggerConfiguration::getLogger(std::string_view name, const std::optional<utils::Identifier>& id) {
  const std::lock_guard<std::mutex> lock(mutex_);
  return getLogger(name, id, lock);
}

LoggerConfiguration::LoggerId LoggerConfiguration::calculateLoggerId(std::string_view name, const std::optional<utils::Identifier>& id) const {
  std::string adjusted_name{name};
  const std::string clazz = "class ";
  const auto haz_clazz = name.find(clazz);
  if (haz_clazz == 0)
    adjusted_name = name.substr(clazz.length(), name.length() - clazz.length());
  if (shorten_names_) {
    utils::ClassUtils::shortenClassName(adjusted_name, adjusted_name);
  }
  return LoggerId{.name = adjusted_name, .uuid = include_uuid_ ? id : std::nullopt};
}

std::shared_ptr<Logger> LoggerConfiguration::getLogger(std::string_view name, const std::optional<utils::Identifier>& id, const std::lock_guard<std::mutex>& lock) {
  const auto logger_id = calculateLoggerId(name, id);

  std::shared_ptr<LoggerImpl> result = std::make_shared<LoggerImpl>(logger_id.name, logger_id.uuid, controller_, get_logger(lock, root_namespace_, logger_id.name, formatter_));
  if (max_log_entry_length_) {
    result->set_max_log_size(gsl::narrow<int>(*max_log_entry_length_));
  }
  return result;
}

std::shared_ptr<spdlog::logger> LoggerConfiguration::getSpdlogLogger(const std::string& name) {
  return spdlog::get(name);
}

std::shared_ptr<internal::LoggerNamespace> LoggerConfiguration::initialize_namespaces(const std::shared_ptr<LoggerProperties> &logger_properties, const std::shared_ptr<Logger> &logger) {
  std::map<std::string, std::shared_ptr<spdlog::sinks::sink>> sink_map = logger_properties->initial_sinks();

  std::string appender = "appender";
  for (auto const & appender_key : logger_properties->get_keys_of_type(appender)) {
    std::string appender_name = appender_key.substr(appender.length() + 1);
    std::string appender_type;
    if (!logger_properties->getString(appender_key, appender_type)) {
      appender_type = "stderr";
    }
    ranges::transform(appender_type, appender_type.begin(), ::tolower);

    if ("nullappender" == appender_type || "null appender" == appender_type || "null" == appender_type) {
      sink_map[appender_name] = std::make_shared<spdlog::sinks::null_sink_st>();
    } else if ("rollingappender" == appender_type || "rolling appender" == appender_type || "rolling" == appender_type) {
      sink_map[appender_name] = getRotatingFileSink(appender_key, logger_properties);
    } else if ("stdout" == appender_type) {
      sink_map[appender_name] = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    } else if ("stderr" == appender_type) {
      sink_map[appender_name] = std::make_shared<spdlog::sinks::stderr_sink_mt>();
    } else if ("syslog" == appender_type) {
      sink_map[appender_name] = LoggerConfiguration::create_syslog_sink();
    } else if ("alert" == appender_type) {
      if (auto sink = AlertSink::create(appender_key, logger_properties, logger)) {
        sink_map[appender_name] = sink;
      }
    } else {
      sink_map[appender_name] = LoggerConfiguration::create_fallback_sink();
    }
  }

  std::shared_ptr<internal::LoggerNamespace> root_namespace = std::make_shared<internal::LoggerNamespace>();
  std::string logger_type = "logger";
  for (auto const & logger_key : logger_properties->get_keys_of_type(logger_type)) {
    std::string logger_def;
    if (!logger_properties->getString(logger_key, logger_def)) {
      continue;
    }
    bool first = true;
    spdlog::level::level_enum level = spdlog::level::info;
    std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;
    for (auto const & segment : utils::string::split(logger_def, ",")) {
      std::string level_name = utils::string::trim(segment);
      if (first) {
        first = false;
        if (auto opt_level = utils::parse_log_level(level_name)) {
          level = *opt_level;
        }
      } else {
        if (auto it = sink_map.find(level_name); it != sink_map.end()) {
          sinks.push_back(it->second);
        } else {
          logger->log_error("Couldn't find sink '{}'", level_name);
        }
      }
    }
    std::shared_ptr<internal::LoggerNamespace> current_namespace = root_namespace;
    if (logger_key != "logger.root") {
      for (auto const & name : utils::string::split(logger_key.substr(logger_type.length() + 1, logger_key.length() - logger_type.length()), "::")) {
        auto child_pair = current_namespace->children.find(name);
        std::shared_ptr<internal::LoggerNamespace> child;
        if (child_pair == current_namespace->children.end()) {
          child = std::make_shared<internal::LoggerNamespace>();
          current_namespace->children[name] = child;
        } else {
          child = child_pair->second;
        }
        current_namespace = child;
      }
    }
    current_namespace->level = level;
    current_namespace->has_level = true;
    current_namespace->sinks = sinks;
  }
  return root_namespace;
}

std::shared_ptr<spdlog::logger> LoggerConfiguration::get_logger(const std::lock_guard<std::mutex>& lock,
    const std::shared_ptr<internal::LoggerNamespace> &root_namespace,
    const std::string& name,
    const std::shared_ptr<spdlog::formatter>& formatter) {
  if (auto spdlogger = spdlog::get(name)) {
    return spdlogger;
  }
  return create_logger(lock, root_namespace, name, formatter);
}

void LoggerConfiguration::setupSpdLogger(const std::lock_guard<std::mutex>&,
    const std::shared_ptr<spdlog::logger>& spd_logger,
    const std::shared_ptr<internal::LoggerNamespace>& root_namespace,
    const std::string& name,
    const std::shared_ptr<spdlog::formatter>& formatter) {
  if (!spd_logger)
    return;
  std::shared_ptr<internal::LoggerNamespace> current_namespace = root_namespace;
  std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks = root_namespace->sinks;
  std::vector<std::shared_ptr<spdlog::sinks::sink>> inherited_sinks;
  spdlog::level::level_enum level = root_namespace->level;
  std::string current_namespace_str;
  for (auto const & name_segment : utils::string::split(name, "::")) {
    current_namespace_str += name_segment;
    auto child_pair = current_namespace->children.find(name_segment);
    if (child_pair == current_namespace->children.end()) {
      break;
    }
    ranges::copy(current_namespace->exported_sinks, std::back_inserter(inherited_sinks));

    current_namespace = child_pair->second;
    if (!current_namespace->sinks.empty()) {
      sinks = current_namespace->sinks;
    }
    if (current_namespace->has_level) {
      level = current_namespace->level;
    }
    current_namespace_str += "::";
  }
  ranges::copy(inherited_sinks, std::back_inserter(sinks));
  spd_logger->sinks() = sinks;
  spd_logger->set_level(level);
  spd_logger->set_formatter(formatter->clone());
  spd_logger->flush_on(std::max(spdlog::level::info, current_namespace->level));
}

std::shared_ptr<spdlog::logger> LoggerConfiguration::create_logger(const std::lock_guard<std::mutex>& lock, const std::shared_ptr<internal::LoggerNamespace>& root_namespace, const std::string& name,
  const std::shared_ptr<spdlog::formatter>& formatter) {
  const auto spd_logger = gsl::make_not_null(std::make_shared<spdlog::logger>(name));
  setupSpdLogger(lock, spd_logger, root_namespace, name, formatter);
  try {
    spdlog::register_logger(spd_logger);
  } catch (const spdlog::spdlog_ex &) {
    // Ignore as someone else beat us to registration, we should get the one they made below
  }
  return spdlog::get(name);
}

spdlog::sink_ptr LoggerConfiguration::create_syslog_sink() {
#ifdef WIN32
  return std::make_shared<internal::windowseventlog_sink>("ApacheNiFiMiNiFi");
#else
  return std::dynamic_pointer_cast<spdlog::sinks::sink>(spdlog::syslog_logger_mt("ApacheNiFiMiNiFi", "", 0, LOG_USER, false));
#endif
}

spdlog::sink_ptr LoggerConfiguration::create_fallback_sink() {
  if (utils::Environment::isRunningAsService()) {
    return LoggerConfiguration::create_syslog_sink();
  } else {
    return std::dynamic_pointer_cast<spdlog::sinks::sink>(std::make_shared<spdlog::sinks::stderr_sink_mt>());
  }
}

std::shared_ptr<internal::LoggerNamespace> LoggerConfiguration::create_default_root() {
  std::shared_ptr<internal::LoggerNamespace> result = std::make_shared<internal::LoggerNamespace>();
  result->sinks = { std::make_shared<spdlog::sinks::stderr_sink_mt>() };
  result->level = spdlog::level::info;
  return result;
}

void LoggerConfiguration::initializeCompression(const std::lock_guard<std::mutex>& lock, const std::shared_ptr<LoggerProperties>& properties) {
  if (const auto compression_sink = compression_manager_.initialize(properties, logger_, [&] (const std::string& name) {return getLogger(name, std::nullopt, lock);})) {
    root_namespace_->sinks.push_back(compression_sink);
    root_namespace_->exported_sinks.push_back(compression_sink);
  }
}

void LoggerConfiguration::initializeAlertSinks(const std::shared_ptr<Configure>& config) {
  auto ssl_service = controllers::SSLContextService::createAndEnable("AlertSinkSSLContextService", config);
  if (ssl_service->getCertificateFile().empty()) {
    ssl_service.reset();
  }
  std::lock_guard guard(mutex_);
  for (auto& sink : alert_sinks_) {
    sink->initialize(config, ssl_service);
  }
}

std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> LoggerConfiguration::getRotatingFileSink(const std::string& appender_key, const std::shared_ptr<LoggerProperties>& properties) {
  // According to spdlog docs, if two loggers write to the same file, they must use the same sink object.
  // Note that some logging configuration changes will not take effect until MiNiFi is restarted.
  static std::map<std::filesystem::path, std::shared_ptr<spdlog::sinks::rotating_file_sink_mt>> rotating_file_sinks;
  static std::mutex sink_map_mtx;

  std::string file_name_str;
  if (!properties->getString(appender_key + ".file_name", file_name_str)) {
    file_name_str = "minifi-app.log";
  }
  std::string directory_str;
  if (!properties->getString(appender_key + ".directory", directory_str)) {
    // The below part assumes logger_properties->getHome() is existing
    // Cause minifiHome must be set at MiNiFiMain.cpp?
    directory_str = properties->getDefaultLogDir().string();
  }

  auto file_name = std::filesystem::path(directory_str) / file_name_str;
  if (utils::file::FileUtils::create_dir(directory_str) == -1) {
    std::cerr << directory_str << " cannot be created\n";
    exit(1);
  }

  int max_files = 3;
  std::string max_files_str;
  if (properties->getString(appender_key + ".max_files", max_files_str)) {
    try {
      max_files = std::stoi(max_files_str);
    } catch (const std::invalid_argument &) {
    } catch (const std::out_of_range &) {
    }
  }

  size_t max_file_size = 5_MiB;
  std::string max_file_size_str;
  if (properties->getString(appender_key + ".max_file_size", max_file_size_str)) {
    core::DataSizeValue::StringToInt(max_file_size_str, max_file_size);
  }

  std::lock_guard<std::mutex> guard(sink_map_mtx);
  if (const auto it = rotating_file_sinks.find(file_name); it != rotating_file_sinks.end()) {
    return it->second;
  }
  auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_name.string(), max_file_size, max_files);
  rotating_file_sinks.emplace(file_name, sink);
  return sink;
}

}  // namespace org::apache::nifi::minifi::core::logging
