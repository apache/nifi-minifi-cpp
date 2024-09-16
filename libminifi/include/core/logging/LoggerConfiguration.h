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
#pragma once

#include <memory>
#include <utility>
#include <vector>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

#include "spdlog/common.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/sink.h"
#include "spdlog/logger.h"
#include "spdlog/formatter.h"

#include "core/Core.h"
#include "core/logging/LoggerBase.h"
#include "LoggerProperties.h"
#include "internal/CompressionManager.h"
#include "alert/AlertSink.h"

class LoggerTestAccessor;

class LogTestController;

namespace org::apache::nifi::minifi::core::logging {

namespace internal {
struct LoggerNamespace {
  spdlog::level::level_enum level{spdlog::level::off};
  bool has_level{false};
  std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;
  // sinks made available to all descendants
  std::vector<std::shared_ptr<spdlog::sinks::sink>> exported_sinks;
  std::map<std::string, std::shared_ptr<LoggerNamespace>> children;

  void forEachSink(const std::function<void(const std::shared_ptr<spdlog::sinks::sink>&)>& op) const;
};

inline std::optional<std::string> formatId(const std::optional<utils::Identifier>& opt_id) {
  return opt_id | utils::transform([](auto id) { return " (" + std::string(id.to_string()) + ")"; });
}

inline constexpr std::string_view UNLIMITED_LOG_ENTRY_LENGTH = "unlimited";
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

  static std::vector<std::unique_ptr<io::InputStream>> getCompressedLogs() {
    return getCompressedLogs(std::chrono::milliseconds{0});
  }

  void initializeAlertSinks(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<AgentIdentificationProvider>& agent_id);

  template<class Rep, class Period>
  static std::vector<std::unique_ptr<io::InputStream>> getCompressedLogs(const std::chrono::duration<Rep, Period>& time) {
    return getConfiguration().compression_manager_.getCompressedLogs(time);
  }

  /**
   * Can be used to get arbitrarily named Logger, LoggerFactory should be preferred within a class.
   */
  std::shared_ptr<Logger> getLogger(std::string_view name, const std::optional<utils::Identifier>& id = {});

  static const char *spdlog_default_pattern;

 protected:
  static std::shared_ptr<internal::LoggerNamespace> initialize_namespaces(const std::shared_ptr<LoggerProperties> &logger_properties, const std::shared_ptr<Logger> &logger = {});
  static std::shared_ptr<spdlog::logger> get_logger(const std::lock_guard<std::mutex>&,
      const std::shared_ptr<internal::LoggerNamespace> &root_namespace,
      const std::string& name,
      const std::shared_ptr<spdlog::formatter>& formatter);
  static std::shared_ptr<spdlog::logger> create_logger(const std::lock_guard<std::mutex>&,
      const std::shared_ptr<internal::LoggerNamespace> &root_namespace,
      const std::string& name,
      const std::shared_ptr<spdlog::formatter>& formatter);
  static void setupSpdLogger(const std::lock_guard<std::mutex>&,
      const std::shared_ptr<spdlog::logger>& spd_logger,
      const std::shared_ptr<internal::LoggerNamespace> &root_namespace,
      const std::string& name,
      const std::shared_ptr<spdlog::formatter>& formatter);

 private:
  std::shared_ptr<Logger> getLogger(std::string_view name, const std::optional<utils::Identifier>& id, const std::lock_guard<std::mutex>& lock);

  void initializeCompression(const std::lock_guard<std::mutex>& lock, const std::shared_ptr<LoggerProperties>& properties);

  static spdlog::sink_ptr create_syslog_sink();
  static spdlog::sink_ptr create_fallback_sink();

  static std::shared_ptr<internal::LoggerNamespace> create_default_root();

  static std::shared_ptr<spdlog::logger> getSpdlogLogger(const std::string& name);

  class LoggerImpl : public LoggerBase {
   public:
    explicit LoggerImpl(std::string name, const std::optional<utils::Identifier>& id, const std::shared_ptr<LoggerControl> &controller, const std::shared_ptr<spdlog::logger> &delegate)
        : LoggerBase(delegate, controller),
          name(std::move(name)),
          id(internal::formatId(id)) {
    }

    void set_delegate(std::shared_ptr<spdlog::logger> delegate) {
      std::lock_guard<std::mutex> lock(mutex_);
      delegate_ = std::move(delegate);
    }

    std::optional<std::string> get_id() override { return id; }

    std::string name;
    std::optional<std::string> id;
  };

  static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> getRotatingFileSink(const std::string& appender_key, const std::shared_ptr<LoggerProperties>& properties);

  LoggerConfiguration();
  internal::CompressionManager compression_manager_;
  std::shared_ptr<internal::LoggerNamespace> root_namespace_;

  struct LoggerId {
    std::string name;
    std::optional<utils::Identifier> uuid;
  };
  LoggerId calculateLoggerId(std::string_view name, const std::optional<utils::Identifier>& id) const;

  std::shared_ptr<spdlog::formatter> formatter_;
  std::mutex mutex_;
  std::shared_ptr<LoggerImpl> logger_ = nullptr;
  std::shared_ptr<LoggerControl> controller_;
  std::unordered_set<std::shared_ptr<AlertSink>> alert_sinks_;
  std::optional<int> max_log_entry_length_;
  bool shorten_names_ = false;
  bool include_uuid_ = true;
};

}  // namespace org::apache::nifi::minifi::core::logging
