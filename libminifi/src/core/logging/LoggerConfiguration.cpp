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

#include <algorithm>
#include <vector>
#include <queue>
#include <memory>
#include <map>
#include <string>

#include "core/logging/LoggerConfiguration.h"
#include "utils/StringUtils.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/null_sink.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

std::shared_ptr< internal::LoggerImpl > internal::LoggerImpl::create(const std::string &name) {
  std::shared_ptr<LoggerImpl> logger = std::shared_ptr<LoggerImpl>(new internal::LoggerImpl(name));
  LoggerConfiguration::getConfiguration().init_logger(logger);
  return logger;
}

std::vector< std::string > LoggerProperties::get_keys_of_type(const std::string &type) {
  std::vector<std::string> appenders;
  std::string prefix = type + ".";
  for (auto const & entry : properties_) {
    if (utils::StringUtils::starts_with(entry.first, prefix) &&
      entry.first.find(".", prefix.length() + 1) == std::string::npos) {
      appenders.push_back(entry.first);
    }
  }
  return appenders;
}

void LoggerConfiguration::initialize(const std::shared_ptr<LoggerProperties> &logger_properties)  {
  std::lock_guard<std::mutex> lock(mutex);
  root_namespace_ = initialize_namespaces(logger_properties);
  std::map<std::string, std::shared_ptr<spdlog::logger>> spdloggers;
  for (auto const & logger_impl : loggers) {
    std::shared_ptr<spdlog::logger> spdlogger;
    auto it = spdloggers.find(logger_impl->name);
    if (it == spdloggers.end()) {
      spdlogger = get_logger(logger_, root_namespace_, logger_impl->name, true);
      spdloggers[logger_impl->name] = spdlogger;
    } else {
      spdlogger = it->second;
    }
    logger_impl->set_delegate(spdlogger);
  }
}

void LoggerConfiguration::init_logger(std::shared_ptr<internal::LoggerImpl> logger_impl) {
  std::lock_guard<std::mutex> lock(mutex);
  loggers.push_back(logger_impl);
  logger_impl->set_delegate(get_logger(logger_, root_namespace_, logger_impl->name));
}

std::shared_ptr<internal::LoggerNamespace> LoggerConfiguration::
     initialize_namespaces(const std::shared_ptr<LoggerProperties> &logger_properties) {
  std::map<std::string, std::shared_ptr<spdlog::sinks::sink>> sink_map = logger_properties->initial_sinks();

  std::string appender_type = "appender";
  for (auto const & appender_key : logger_properties->get_keys_of_type(appender_type)) {
    std::string appender_name = appender_key.substr(appender_type.length() + 1);
    std::string appender_type;
    if (!logger_properties->get(appender_key, appender_type)) {
      appender_type = "stderr";
    }
    std::transform(appender_type.begin(), appender_type.end(), appender_type.begin(), ::tolower);

    if ("nullappender" == appender_type || "null appender" == appender_type || "null" == appender_type) {
      sink_map[appender_name] = std::make_shared<spdlog::sinks::null_sink_st>();
    } else if ("rollingappender" == appender_type || "rolling appender" == appender_type || "rolling" == appender_type) {
      std::string file_name = "";
      if (!logger_properties->get(appender_key + ".file_name", file_name)) {
        file_name = "minifi-app.log";
      }

      int max_files = 3;
      std::string max_files_str = "";
      if (logger_properties->get(appender_key + ".max_files", max_files_str)) {
        try {
          max_files = std::stoi(max_files_str);
        } catch (const std::invalid_argument &ia) {
        } catch (const std::out_of_range &oor) {}
      }

      int max_file_size = 5*1024*1024;
      std::string max_file_size_str = "";
      if (logger_properties->get(appender_key + ".max_file_size", max_file_size_str)) {
        try {
          max_file_size = std::stoi(max_file_size_str);
        } catch (const std::invalid_argument &ia) {
        } catch (const std::out_of_range &oor) {}
      }
      sink_map[appender_name] = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_name, max_file_size, max_files);
    } else if ("stdout" == appender_type) {
      sink_map[appender_name] = spdlog::sinks::stdout_sink_mt::instance();
    } else {
      sink_map[appender_name] = spdlog::sinks::stderr_sink_mt::instance();
    }
  }

  std::shared_ptr<internal::LoggerNamespace> root_namespace = std::make_shared<internal::LoggerNamespace>();
  std::string logger_type = "logger";
  for (auto const & logger_key : logger_properties->get_keys_of_type(logger_type)) {
    std::string logger_def;
    if (!logger_properties->get(logger_key, logger_def)) {
      continue;
    }
    bool first = true;
    spdlog::level::level_enum level = spdlog::level::info;
    std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;
    for (auto const & segment : utils::StringUtils::split(logger_def, ",")) {
      std::string level_name = utils::StringUtils::trim(segment);
      if (first) {
        first = false;
        std::transform(level_name.begin(), level_name.end(), level_name.begin(), ::tolower);
        if ("trace" == level_name) {
          level = spdlog::level::trace;
        } else if ("debug" == level_name) {
          level = spdlog::level::debug;
        } else if ("warn" == level_name) {
          level = spdlog::level::warn;
        } else if ("critical" == level_name) {
          level = spdlog::level::critical;
        } else if ("error" == level_name) {
          level = spdlog::level::err;
        } else if ("off" == level_name) {
          level = spdlog::level::off;
        }
      } else {
        sinks.push_back(sink_map[level_name]);
      }
    }
    std::shared_ptr<internal::LoggerNamespace> current_namespace = root_namespace;
    if (logger_key != "logger.root") {
      for (auto const & name : utils::StringUtils::split(logger_key.substr(logger_type.length() + 1,
          logger_key.length() - logger_type.length()), "::")) {
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

std::shared_ptr<spdlog::logger> LoggerConfiguration::get_logger(std::shared_ptr<Logger> logger, const std::shared_ptr<internal::LoggerNamespace> &root_namespace,
                                                                const std::string &name, bool remove_if_present) {
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
    logger->log_debug("%s logger got sinks from namespace %s and level %s from namespace %s", name, sink_namespace_str, spdlog::level::level_names[level], level_namespace_str);
  }
  spdlogger = std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
  spdlogger->set_level(level);
  spdlogger->flush_on(std::max(spdlog::level::info, current_namespace->level));
  try {
    spdlog::register_logger(spdlogger);
  } catch (const spdlog::spdlog_ex &ex) {
    // Ignore as someone else beat us to registration, we should get the one they made below
  }
  return spdlog::get(name);
}

std::shared_ptr<internal::LoggerNamespace> LoggerConfiguration::create_default_root() {
  std::shared_ptr<internal::LoggerNamespace> result = std::make_shared<internal::LoggerNamespace>();
  result->sinks = std::vector<std::shared_ptr<spdlog::sinks::sink>>();
  result->sinks.push_back(spdlog::sinks::stderr_sink_mt::instance());
  result->level = spdlog::level::info;
  return result;
}

} /* namespace logging */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
