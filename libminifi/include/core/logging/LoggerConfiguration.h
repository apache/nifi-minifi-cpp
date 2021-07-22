/**
 * @file LoggerConfiguration.h
 * Logger class declaration
 * This is a C++ wrapper for spdlog, a lightweight C++ logging library
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
#ifndef LIBMINIFI_INCLUDE_CORE_LOGGING_LOGGERCONFIGURATION_H_
#define LIBMINIFI_INCLUDE_CORE_LOGGING_LOGGERCONFIGURATION_H_

#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <string>

#include "spdlog/common.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/sink.h"
#include "spdlog/logger.h"
#include "spdlog/formatter.h"
#include "spdlog/pattern_formatter.h"

#include "core/Core.h"
#include "core/logging/Logger.h"
#include "LoggerProperties.h"
#include "internal/CompressionManager.h"

class LoggerTestAccessor;

class LogTestController;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

namespace internal {
struct LoggerNamespace {
  spdlog::level::level_enum level;
  bool has_level;
  std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;
  // sinks made available to all descendants
  std::vector<std::shared_ptr<spdlog::sinks::sink>> exported_sinks;
  std::map<std::string, std::shared_ptr<LoggerNamespace>> children;

  LoggerNamespace()
      : level(spdlog::level::off),
        has_level(false),
        sinks(std::vector<std::shared_ptr<spdlog::sinks::sink>>()),
        children(std::map<std::string, std::shared_ptr<LoggerNamespace>>()) {
  }
};
}  // namespace internal

class LoggerConfiguration {
  friend class ::LoggerTestAccessor;
  friend class ::LogTestController;

 public:
  /**
   * Gets the current log configuration
   */
  static LoggerConfiguration& getConfiguration();

  static std::unique_ptr<LoggerConfiguration> newInstance() {
    return std::unique_ptr<LoggerConfiguration>(new LoggerConfiguration());
  }

  void disableLogging() {
    controller_->setEnabled(false);
  }

  void enableLogging() {
    controller_->setEnabled(true);
  }

  bool shortenClassNames() const {
    return shorten_names_;
  }
  /**
   * (Re)initializes the logging configuation with the given logger properties.
   */
  void initialize(const std::shared_ptr<LoggerProperties> &logger_properties);

  static std::unique_ptr<io::InputStream> getCompressedLog(bool flush = false) {
    return getCompressedLog(std::chrono::milliseconds{0}, flush);
  }

  template<class Rep, class Period>
  static std::unique_ptr<io::InputStream> getCompressedLog(const std::chrono::duration<Rep, Period>& time, bool flush = false) {
    return getConfiguration().compression_manager_.getCompressedLog(time, flush);
  }

  /**
   * Can be used to get arbitrarily named Logger, LoggerFactory should be preferred within a class.
   */
  std::shared_ptr<Logger> getLogger(const std::string &name);

  static const char *spdlog_default_pattern;

 protected:
  static std::shared_ptr<internal::LoggerNamespace> initialize_namespaces(const std::shared_ptr<LoggerProperties> &logger_properties);
  static std::shared_ptr<spdlog::logger> get_logger(std::shared_ptr<Logger> logger, const std::shared_ptr<internal::LoggerNamespace> &root_namespace, const std::string &name,
                                                    std::shared_ptr<spdlog::formatter> formatter, bool remove_if_present = false);

 private:
  std::shared_ptr<Logger> getLogger(const std::string& name, const std::lock_guard<std::mutex>& lock);

  void initializeCompression(const std::lock_guard<std::mutex>& lock, const std::shared_ptr<LoggerProperties>& properties);

  static spdlog::sink_ptr create_syslog_sink();
  static spdlog::sink_ptr create_fallback_sink();

  static std::shared_ptr<internal::LoggerNamespace> create_default_root();

  static std::shared_ptr<spdlog::logger> getSpdlogLogger(const std::string& name);

  class LoggerImpl : public Logger {
   public:
    explicit LoggerImpl(const std::string &name, const std::shared_ptr<LoggerControl> &controller, const std::shared_ptr<spdlog::logger> &delegate)
        : Logger(delegate, controller),
          name(name) {
    }

    void set_delegate(std::shared_ptr<spdlog::logger> delegate) {
      std::lock_guard<std::mutex> lock(mutex_);
      delegate_ = delegate;
    }
    const std::string name;
  };

  LoggerConfiguration();
  internal::CompressionManager compression_manager_;
  std::shared_ptr<internal::LoggerNamespace> root_namespace_;
  std::vector<std::shared_ptr<LoggerImpl>> loggers;
  std::shared_ptr<spdlog::formatter> formatter_;
  std::mutex mutex;
  std::shared_ptr<LoggerImpl> logger_ = nullptr;
  std::shared_ptr<LoggerControl> controller_;
  bool shorten_names_;
};

template<typename T>
class LoggerFactory {
 public:
  /**
   * Gets an initialized logger for the template class.
   */
  static std::shared_ptr<Logger> getLogger() {
    static std::shared_ptr<Logger> logger = LoggerConfiguration::getConfiguration().getLogger(core::getClassName<T>());
    return logger;
  }

  static std::shared_ptr<Logger> getAliasedLogger(const std::string &alias) {
    std::shared_ptr<Logger> logger = LoggerConfiguration::getConfiguration().getLogger(alias);
    return logger;
  }
};

}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_LOGGING_LOGGERCONFIGURATION_H_
