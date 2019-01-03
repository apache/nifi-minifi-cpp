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
#ifndef __LOGGER_CONFIGURATION_H__
#define __LOGGER_CONFIGURATION_H__

#include <map>
#include <mutex>
#include <string>
#include "spdlog/spdlog.h"
#include "spdlog/formatter.h"

#include "core/Core.h"
#include "core/logging/Logger.h"
#include "properties/Properties.h"

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
  std::map<std::string, std::shared_ptr<LoggerNamespace>> children;

  LoggerNamespace()
      : level(spdlog::level::off),
        has_level(false),
        sinks(std::vector<std::shared_ptr<spdlog::sinks::sink>>()),
        children(std::map<std::string, std::shared_ptr<LoggerNamespace>>()) {
  }
};
}
;

class LoggerProperties : public Properties {
 public:
  /**
   * Gets all keys that start with the given prefix and do not have a "." after the prefix and "." separator.
   *
   * Ex: with type argument "appender"
   * you would get back a property of "appender.rolling" but not "appender.rolling.file_name"
   */
  std::vector<std::string> get_keys_of_type(const std::string &type);

  /**
   * Registers a sink witht the given name. This allows for programmatic definition of sinks.
   */
  void add_sink(const std::string &name, std::shared_ptr<spdlog::sinks::sink> sink) {
    sinks_[name] = sink;
  }
  std::map<std::string, std::shared_ptr<spdlog::sinks::sink>> initial_sinks() {
    return sinks_;
  }

  static const char* appender_prefix;
  static const char* logger_prefix;
 private:
  std::map<std::string, std::shared_ptr<spdlog::sinks::sink>> sinks_;
};

class LoggerConfiguration {
 public:
  /**
   * Gets the current log configuration
   */
  static LoggerConfiguration& getConfiguration() {
    static LoggerConfiguration logger_configuration;
    return logger_configuration;
  }

  void disableLogging(){
    controller_->setEnabled(false);
  }

  void enableLogging(){
      controller_->setEnabled(true);
    }
  /**
   * (Re)initializes the logging configuation with the given logger properties.
   */
  void initialize(const std::shared_ptr<LoggerProperties> &logger_properties);

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
  static std::shared_ptr<internal::LoggerNamespace> create_default_root();

  class LoggerImpl : public Logger {
   public:
    LoggerImpl(std::string name, std::shared_ptr<LoggerControl> controller, std::shared_ptr<spdlog::logger> delegate)
        : Logger(delegate,controller),
          name(name) {
    }
    void set_delegate(std::shared_ptr<spdlog::logger> delegate) {
      std::lock_guard<std::mutex> lock(mutex_);
      delegate_ = delegate;
    }
    const std::string name;

  };

  LoggerConfiguration();
  std::shared_ptr<internal::LoggerNamespace> root_namespace_;
  std::vector<std::shared_ptr<LoggerImpl>> loggers;
  std::shared_ptr<spdlog::formatter> formatter_;
  std::mutex mutex;
  std::shared_ptr<LoggerImpl> logger_ = nullptr;
  std::shared_ptr<LoggerControl> controller_;
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

} /* namespace logging */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
