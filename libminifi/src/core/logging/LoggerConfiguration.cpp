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
#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "core/Core.h"
#include "utils/StringUtils.h"
#include "utils/ClassUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/Environment.h"
#include "core/logging/internal/LogCompressorSink.h"
#include "utils/Literals.h"
#include "core/TypedValues.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/null_sink.h"

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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

const char* LoggerConfiguration::spdlog_default_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v";

namespace {
std::optional<spdlog::level::level_enum> parse_log_level(const std::string& level_name) {
  if (utils::StringUtils::equalsIgnoreCase(level_name, "trace")) {
    return spdlog::level::trace;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "debug")) {
    return spdlog::level::debug;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "info")) {
    return spdlog::level::info;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "warn")) {
    return spdlog::level::warn;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "error")) {
    return spdlog::level::err;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "critical")) {
    return spdlog::level::critical;
  } else if (utils::StringUtils::equalsIgnoreCase(level_name, "off")) {
    return spdlog::level::off;
  }
  return std::nullopt;
}
}  // namespace

std::vector<std::string> LoggerProperties::get_keys_of_type(const std::string &type) {
  std::vector<std::string> appenders;
  std::string prefix = type + ".";
  for (auto const & entry : getProperties()) {
    if (entry.first.rfind(prefix, 0) == 0 && entry.first.find(".", prefix.length() + 1) == std::string::npos) {
      appenders.push_back(entry.first);
    }
  }
  return appenders;
}

LoggerConfiguration::LoggerConfiguration()
    : root_namespace_(create_default_root()),
      loggers(std::vector<std::shared_ptr<LoggerImpl>>()),
      formatter_(std::make_shared<spdlog::pattern_formatter>(spdlog_default_pattern)),
      shorten_names_(false) {
  controller_ = std::make_shared<LoggerControl>();
  logger_ = std::shared_ptr<LoggerImpl>(
      new LoggerImpl(core::getClassName<LoggerConfiguration>(), controller_, get_logger(nullptr, root_namespace_, core::getClassName<LoggerConfiguration>(), formatter_)));
  loggers.push_back(logger_);
}

LoggerConfiguration& LoggerConfiguration::getConfiguration() {
  static LoggerConfiguration instance;
  return instance;
}

void LoggerConfiguration::initialize(const std::shared_ptr<LoggerProperties> &logger_properties) {
  std::lock_guard<std::mutex> lock(mutex);
  root_namespace_ = initialize_namespaces(logger_properties);
  initializeCompression(lock, logger_properties);
  std::string spdlog_pattern;
  if (!logger_properties->getString("spdlog.pattern", spdlog_pattern)) {
    spdlog_pattern = spdlog_default_pattern;
  }

  /**
   * There is no need to shorten names per spdlog sink as this is a per log instance.
   */
  std::string shorten_names_str;
  if (logger_properties->getString("spdlog.shorten_names", shorten_names_str)) {
    shorten_names_ = utils::StringUtils::toBool(shorten_names_str).value_or(false);
  }

  formatter_ = std::make_shared<spdlog::pattern_formatter>(spdlog_pattern);
  std::map<std::string, std::shared_ptr<spdlog::logger>> spdloggers;
  for (auto const & logger_impl : loggers) {
    std::shared_ptr<spdlog::logger> spdlogger;
    auto it = spdloggers.find(logger_impl->name);
    if (it == spdloggers.end()) {
      spdlogger = get_logger(logger_, root_namespace_, logger_impl->name, formatter_, true);
      spdloggers[logger_impl->name] = spdlogger;
    } else {
      spdlogger = it->second;
    }
    logger_impl->set_delegate(spdlogger);
  }
  logger_->log_debug("Set following pattern on loggers: %s", spdlog_pattern);
}

std::shared_ptr<Logger> LoggerConfiguration::getLogger(const std::string &name) {
  std::lock_guard<std::mutex> lock(mutex);
  return getLogger(name, lock);
}

std::shared_ptr<Logger> LoggerConfiguration::getLogger(const std::string &name, const std::lock_guard<std::mutex>& /*lock*/) {
  std::string adjusted_name = name;
  const std::string clazz = "class ";
  auto haz_clazz = name.find(clazz);
  if (haz_clazz == 0)
    adjusted_name = name.substr(clazz.length(), name.length() - clazz.length());
  if (shorten_names_) {
    utils::ClassUtils::shortenClassName(adjusted_name, adjusted_name);
  }

  std::shared_ptr<LoggerImpl> result = std::make_shared<LoggerImpl>(adjusted_name, controller_, get_logger(logger_, root_namespace_, adjusted_name, formatter_));
  loggers.push_back(result);
  return result;
}

std::shared_ptr<spdlog::logger> LoggerConfiguration::getSpdlogLogger(const std::string& name) {
  return spdlog::get(name);
}

std::shared_ptr<internal::LoggerNamespace> LoggerConfiguration::initialize_namespaces(const std::shared_ptr<LoggerProperties> &logger_properties) {
  std::map<std::string, std::shared_ptr<spdlog::sinks::sink>> sink_map = logger_properties->initial_sinks();

  std::string appender_type = "appender";
  for (auto const & appender_key : logger_properties->get_keys_of_type(appender_type)) {
    std::string appender_name = appender_key.substr(appender_type.length() + 1);
    std::string appender_type;
    if (!logger_properties->getString(appender_key, appender_type)) {
      appender_type = "stderr";
    }
    std::transform(appender_type.begin(), appender_type.end(), appender_type.begin(), ::tolower);

    if ("nullappender" == appender_type || "null appender" == appender_type || "null" == appender_type) {
      sink_map[appender_name] = std::make_shared<spdlog::sinks::null_sink_st>();
    } else if ("rollingappender" == appender_type || "rolling appender" == appender_type || "rolling" == appender_type) {
      std::string file_name;
      if (!logger_properties->getString(appender_key + ".file_name", file_name)) {
        file_name = "minifi-app.log";
      }
      std::string directory;
      if (!logger_properties->getString(appender_key + ".directory", directory)) {
        // The below part assumes logger_properties->getHome() is existing
        // Cause minifiHome must be set at MiNiFiMain.cpp?
        directory = logger_properties->getHome() + utils::file::FileUtils::get_separator() + "logs";
      }

      if (utils::file::FileUtils::create_dir(directory) == -1) {
        std::cerr << directory << " cannot be created\n";
        exit(1);
      }
      file_name = directory + utils::file::FileUtils::get_separator() + file_name;

      int max_files = 3;
      std::string max_files_str = "";
      if (logger_properties->getString(appender_key + ".max_files", max_files_str)) {
        try {
          max_files = std::stoi(max_files_str);
        } catch (const std::invalid_argument &) {
        } catch (const std::out_of_range &) {
        }
      }

      int max_file_size = 5 * 1024 * 1024;
      std::string max_file_size_str = "";
      if (logger_properties->getString(appender_key + ".max_file_size", max_file_size_str)) {
        try {
          max_file_size = std::stoi(max_file_size_str);
        } catch (const std::invalid_argument &) {
        } catch (const std::out_of_range &) {
        }
      }
      sink_map[appender_name] = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_name, max_file_size, max_files);
    } else if ("stdout" == appender_type) {
      sink_map[appender_name] = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    } else if ("stderr" == appender_type) {
      sink_map[appender_name] = std::make_shared<spdlog::sinks::stderr_sink_mt>();
    } else if ("syslog" == appender_type) {
      sink_map[appender_name] = LoggerConfiguration::create_syslog_sink();
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
    for (auto const & segment : utils::StringUtils::split(logger_def, ",")) {
      std::string level_name = utils::StringUtils::trim(segment);
      if (first) {
        first = false;
        auto opt_level = parse_log_level(level_name);
        if (opt_level) {
          level = *opt_level;
        }
      } else {
        sinks.push_back(sink_map[level_name]);
      }
    }
    std::shared_ptr<internal::LoggerNamespace> current_namespace = root_namespace;
    if (logger_key != "logger.root") {
      for (auto const & name : utils::StringUtils::split(logger_key.substr(logger_type.length() + 1, logger_key.length() - logger_type.length()), "::")) {
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

std::shared_ptr<spdlog::logger> LoggerConfiguration::get_logger(std::shared_ptr<Logger> logger, const std::shared_ptr<internal::LoggerNamespace> &root_namespace, const std::string &name,
                                                                std::shared_ptr<spdlog::formatter> formatter, bool remove_if_present) {
  std::shared_ptr<spdlog::logger> spdlogger = spdlog::get(name);
  if (spdlogger) {
    if (remove_if_present) {
      spdlog::drop(name);
    } else {
      return spdlogger;
    }
  }
  std::shared_ptr<internal::LoggerNamespace> current_namespace = root_namespace;
  std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks = root_namespace->sinks;
  std::vector<std::shared_ptr<spdlog::sinks::sink>> inherited_sinks;
  spdlog::level::level_enum level = root_namespace->level;
  std::string current_namespace_str = "";
  std::string sink_namespace_str = "root";
  std::string level_namespace_str = "root";
  for (auto const & name_segment : utils::StringUtils::split(name, "::")) {
    current_namespace_str += name_segment;
    auto child_pair = current_namespace->children.find(name_segment);
    if (child_pair == current_namespace->children.end()) {
      break;
    }
    std::copy(current_namespace->exported_sinks.begin(), current_namespace->exported_sinks.end(), std::back_inserter(inherited_sinks));
    current_namespace = child_pair->second;
    if (current_namespace->sinks.size() > 0) {
      sinks = current_namespace->sinks;
      sink_namespace_str = current_namespace_str;
    }
    if (current_namespace->has_level) {
      level = current_namespace->level;
      level_namespace_str = current_namespace_str;
    }
    current_namespace_str += "::";
  }
  if (logger != nullptr) {
    const auto levelView(spdlog::level::to_string_view(level));
    logger->log_debug("%s logger got sinks from namespace %s and level %s from namespace %s", name, sink_namespace_str, std::string(levelView.begin(), levelView.end()), level_namespace_str);
  }
  std::copy(inherited_sinks.begin(), inherited_sinks.end(), std::back_inserter(sinks));
  spdlogger = std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
  spdlogger->set_level(level);
  spdlogger->set_formatter(formatter -> clone());
  spdlogger->flush_on(std::max(spdlog::level::info, current_namespace->level));
  try {
    spdlog::register_logger(spdlogger);
  } catch (const spdlog::spdlog_ex &) {
    // Ignore as someone else beat us to registration, we should get the one they made below
  }
  return spdlog::get(name);
}

spdlog::sink_ptr LoggerConfiguration::create_syslog_sink() {
#ifdef WIN32
  return std::make_shared<internal::windowseventlog_sink>("ApacheNiFiMiNiFi");
#else
  return std::dynamic_pointer_cast<spdlog::sinks::sink>(spdlog::syslog_logger_mt("ApacheNiFiMiNiFi", 0, LOG_USER, false));
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
  auto compression_sink = compression_manager_.initialize(properties, logger_, [&] (const std::string& name) {return getLogger(name, lock);});
  if (compression_sink) {
    root_namespace_->sinks.push_back(compression_sink);
    root_namespace_->exported_sinks.push_back(compression_sink);
  }
}

} /* namespace logging */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
