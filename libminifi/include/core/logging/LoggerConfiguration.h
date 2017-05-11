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
#include "core/logging/Logger.h"
#include "properties/Properties.h"

#include "spdlog/spdlog.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

namespace internal {
  /**
   * This class should not be instantiated directly.  Use LoggerFactory to get an instance.
   */
  class LoggerImpl : public Logger {
    public:
     static std::shared_ptr<LoggerImpl> create(const std::string &name);
     void set_delegate(std::shared_ptr<spdlog::logger> delegate) {
       delegate_ = delegate;
     }
     const std::string name;
     LoggerImpl(std::string name):Logger(nullptr), name(name) {}
  };

  struct LoggerNamespace {
    spdlog::level::level_enum level;
    bool has_level;
    std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;
    std::map<std::string, std::shared_ptr<LoggerNamespace>> children;

    LoggerNamespace() : level(spdlog::level::off), has_level(false), sinks(std::vector<std::shared_ptr<spdlog::sinks::sink>>()), children(std::map<std::string, std::shared_ptr<LoggerNamespace>>()) {}
  };
};
 
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

template<typename T>
class LoggerFactory {
 public:
  /**
   * Gets an initialized logger for the template class.
   */
  static std::shared_ptr<Logger> getLogger() {
   static std::shared_ptr<Logger> logger = internal::LoggerImpl::create(core::getClassName<T>());
   return logger;
  }
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

  /**
   * (Re)initializes the logging configuation with the given logger properties.
   * 
   * This should NOT be called while other threads are logging.  It is mainly intended so that the startup
   * configuration can be overridden during initialization of the application once MINIFI_HOME is determined
   * and the LoggerProperties can be loaded.
   */
  void initialize(const std::shared_ptr<LoggerProperties> &logger_properties);

  /**
   * Used by logging framework to register logger implementations
   */
  void init_logger(std::shared_ptr<internal::LoggerImpl> logger_impl);
 protected:
  static std::shared_ptr<internal::LoggerNamespace> initialize_namespaces(const std::shared_ptr<LoggerProperties>  &logger_properties);
  static std::shared_ptr<spdlog::logger> get_logger(std::shared_ptr<Logger> logger, const std::shared_ptr<internal::LoggerNamespace> &root_namespace, const std::string &name, bool remove_if_present = false);
 private:
  static std::shared_ptr<internal::LoggerNamespace> create_default_root();
  LoggerConfiguration() : root_namespace_(create_default_root()), loggers(std::vector<std::shared_ptr<internal::LoggerImpl>>()) {
    logger_ = std::make_shared<internal::LoggerImpl>(core::getClassName<LoggerConfiguration>());
    logger_->set_delegate(get_logger(nullptr, root_namespace_, logger_->name));
    loggers.push_back(logger_);
  }
  std::shared_ptr<internal::LoggerNamespace> root_namespace_;
  std::vector<std::shared_ptr<internal::LoggerImpl>> loggers;
  std::mutex mutex;
  std::shared_ptr<internal::LoggerImpl> logger_ = nullptr;
};

} /* namespace logging */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
