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
#ifndef LIBMINIFI_INCLUDE_BASELOGGER_H_
#define LIBMINIFI_INCLUDE_BASELOGGER_H_

#include <string>
#include <memory>
#include "spdlog/spdlog.h"
#include <iostream>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

// 5M default log file size
#define DEFAULT_LOG_FILE_SIZE (5*1024*1024)
// 3 log files rotation
#define DEFAULT_LOG_FILE_NUMBER 3
#define LOG_NAME "minifi log"
#define LOG_FILE_NAME "minifi-app.log"

/**
 * Log level enumeration.
 */
typedef enum {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  err = 4,
  critical = 5,
  off = 6
} LOG_LEVEL_E;

#define LOG_BUFFER_SIZE 1024

template<typename ... Args>
inline std::string format_string(char const* format_str, Args&&... args) {
  char buf[LOG_BUFFER_SIZE+1] = {0};
  
  std::snprintf(buf, LOG_BUFFER_SIZE, format_str, args...);
  return std::string(buf);
}

inline std::string format_string(char const* format_str) {
  return format_str;
}
inline char const* conditional_conversion(std::string const& str) {
  return str.c_str();
}

template<typename T>
inline T conditional_conversion(T const& t) {
  return t;
}

/**
 * Base class that represents a logger configuration.
 */
class BaseLogger {

 public:
  static const char *nifi_log_level;
  static const char *nifi_log_appender;

  /**
   * Base Constructor
   */
  BaseLogger() {
    setLogLevel("info");
    logger_ = nullptr;
    stderr_ = nullptr;
  }

  /**
   * Logger configuration constructorthat will set the base log level.
   * @param config incoming configuration.
   */
  BaseLogger(std::string log_level, std::shared_ptr<spdlog::logger> logger)
      : logger_(logger) {
    setLogLevel(log_level);

  }

  virtual ~BaseLogger() {

  }

  /**
   * Move constructor that will atomically swap configuration
   * shared pointers.
   */
  BaseLogger(const BaseLogger &&other)
      : configured_level_(other.configured_level_.load()) {
    // must atomically exchange the pointers
    logger_ = std::move(other.logger_);
    set_error_logger(other.stderr_);

  }

  /**
   * Returns the log level for this instance.
   */
  virtual LOG_LEVEL_E getLogLevel() const {
    return configured_level_;
  }

  /**
   * @brief Log error message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_error(const char * const format, Args ... args);
  /**
   * @brief Log warn message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_warn(const char * const format, Args ... args);
  /**
   * @brief Log info message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_info(const char * const format, Args ... args);
  /**
   * @brief Log debug message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_debug(const char * const format, Args ... args);
  /**
   * @brief Log trace message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_trace(const char * const format, Args ... args);

  /**
   * @brief Log error message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  virtual void log_str(LOG_LEVEL_E level, const std::string &buffer);

  /**
   * Sets the log level for this instance based on the string
   * @param level desired log leve.
   * @param defaultLevel default level if we cannot match level.
   */
  virtual void setLogLevel(const std::string &level, LOG_LEVEL_E defaultLevel =
                               info);

  /**
   * Sets the log level atomic and sets it
   * within logger if it can
   * @param level desired log level.
   */
  virtual void setLogLevel(LOG_LEVEL_E level) {
    configured_level_ = level;
    setLogLevel();
  }

  bool shouldLog(LOG_LEVEL_E level) {
    return level >= configured_level_.load(std::memory_order_relaxed);
  }

  /**
   * Move operator overload
   */
  BaseLogger &operator=(const BaseLogger &&other) {
    configured_level_ = (other.configured_level_.load());
    // must atomically exchange the pointers
    logger_ = std::move(other.logger_);
    set_error_logger(other.stderr_);
    return *this;
  }

 protected:

  /**
   * Logger configuration constructorthat will set the base log level.
   * @param config incoming configuration.
   */
  BaseLogger(std::string log_level)
      : logger_(nullptr) {
    setLogLevel(log_level);
  }

  void setLogger(std::shared_ptr<spdlog::logger> logger) {
    logger_ = logger;
  }

  /**
   * Since a thread may be using stderr and it can be null,
   * we must atomically exchange the shared pointers.
   * @param other other shared pointer. can be null ptr
   */
  void set_error_logger(std::shared_ptr<spdlog::logger> other);

  /**
   * Sets the log level on the spdlogger if it is not null.
   */
  void setLogLevel() {
    if (logger_ != nullptr)
      logger_->set_level((spdlog::level::level_enum) configured_level_.load());

  }

  std::atomic<LOG_LEVEL_E> configured_level_;
  std::shared_ptr<spdlog::logger> logger_;
  std::shared_ptr<spdlog::logger> stderr_;
};

/**
 * @brief Log error message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
template<typename ... Args>
void BaseLogger::log_error(const char * const format, Args ... args) {
  if (logger_ == NULL || !logger_->should_log(spdlog::level::level_enum::err))
    return;

  log_str(err, format_string(format, conditional_conversion(args)...));
}
/**
 * @brief Log warn message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
template<typename ... Args>
void BaseLogger::log_warn(const char * const format, Args ... args) {
  if (logger_ == NULL || !logger_->should_log(spdlog::level::level_enum::warn))
    return;

  log_str(warn, format_string(format, conditional_conversion(args)...));
}
/**
 * @brief Log info message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
template<typename ... Args>
void BaseLogger::log_info(const char * const format, Args ... args) {
  if (logger_ == NULL || !logger_->should_log(spdlog::level::level_enum::info))
    return;

  log_str(info, format_string(format, conditional_conversion(args)...));
}
/**
 * @brief Log debug message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
template<typename ... Args>
void BaseLogger::log_debug(const char * const format, Args ... args) {
  if (logger_ == NULL || !logger_->should_log(spdlog::level::level_enum::debug))
    return;

  log_str(debug, format_string(format, conditional_conversion(args)...));
}
/**
 * @brief Log trace message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
template<typename ... Args>
void BaseLogger::log_trace(const char * const format, Args ... args) {
  if (logger_ == NULL || !logger_->should_log(spdlog::level::level_enum::trace))
    return;

  log_str(debug, format_string(format, conditional_conversion(args)...));
}

} /* namespace logging */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_BASELOGGER_H_ */
