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
#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <mutex>
#include <memory>
#include <sstream>
#include <iostream>

#include "spdlog/spdlog.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

#define LOG_BUFFER_SIZE 1024

class LoggerControl {
 public:
  LoggerControl()
      : is_enabled_(true) {

  }

  bool is_enabled(){
    return is_enabled_;
  }

  void setEnabled(bool status){
    is_enabled_ = status;
  }
 protected:
  std::atomic<bool> is_enabled_;
};

template<typename ... Args>
inline std::string format_string(char const* format_str, Args&&... args) {
  char buf[LOG_BUFFER_SIZE];
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

typedef enum {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  err = 4,
  critical = 5,
  off = 6
} LOG_LEVEL;

class BaseLogger {
 public:

  virtual ~BaseLogger() {

  }
  virtual void log_string(LOG_LEVEL level, std::string str) = 0;

  virtual bool should_log(const LOG_LEVEL &level) {
    return true;
  }

};

/**
 * LogBuilder is a class to facilitate using the LOG macros below and an associated put-to operator.
 *
 */
class LogBuilder {
 public:
  LogBuilder(BaseLogger *l, LOG_LEVEL level)
      : ignore(false),
        ptr(l),
        level(level) {
    if (!l->should_log(level)) {
      setIgnore();
    }
  }

  ~LogBuilder() {
    if (!ignore)
      log_string(level);
  }

  void setIgnore() {
    ignore = true;
  }

  void log_string(LOG_LEVEL level) {
    ptr->log_string(level, str.str());
  }

  template<typename T>
  LogBuilder &operator<<(const T &o) {
    if (!ignore)
      str << o;
    return *this;
  }

  bool ignore;
  BaseLogger *ptr;
  std::stringstream str;
  LOG_LEVEL level;
};

class Logger : public BaseLogger {
 public:
  /**
   * @brief Log error message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_error(const char * const format, const Args& ... args) {
    log(spdlog::level::err, format, args...);
  }

  /**
   * @brief Log warn message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_warn(const char * const format, const Args& ... args) {
    log(spdlog::level::warn, format, args...);
  }

  /**
   * @brief Log info message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_info(const char * const format, const Args& ... args) {
    log(spdlog::level::info, format, args...);
  }

  /**
   * @brief Log debug message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_debug(const char * const format, const Args& ... args) {
    log(spdlog::level::debug, format, args...);
  }

  /**
   * @brief Log trace message
   * @param format format string ('man printf' for syntax)
   * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
   */
  template<typename ... Args>
  void log_trace(const char * const format, const Args& ... args) {
    log(spdlog::level::trace, format, args...);
  }

  bool should_log(const LOG_LEVEL &level) {
    if (controller_ && !controller_->is_enabled())
      return false;
    spdlog::level::level_enum logger_level = spdlog::level::level_enum::info;
    switch (level) {
      case critical:
        logger_level = spdlog::level::level_enum::critical;
        break;
      case err:
        logger_level = spdlog::level::level_enum::err;
        break;
      case info:
        break;
      case debug:
        logger_level = spdlog::level::level_enum::debug;
        break;
      case off:
        logger_level = spdlog::level::level_enum::off;
        break;
      case trace:
        logger_level = spdlog::level::level_enum::trace;
        break;
      case warn:
        logger_level = spdlog::level::level_enum::warn;
        break;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!delegate_->should_log(logger_level)) {
      return false;
    }
    return true;
  }

 protected:

  virtual void log_string(LOG_LEVEL level, std::string str) {
    switch (level) {
      case critical:
        log_warn(str.c_str());
        break;
      case err:
        log_error(str.c_str());
        break;
      case info:
        log_info(str.c_str());
        break;
      case debug:
        log_debug(str.c_str());
        break;
      case trace:
        log_trace(str.c_str());
        break;
      case warn:
        log_warn(str.c_str());
        break;
      case off:
        break;
    }
  }
  Logger(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller)
      : delegate_(delegate), controller_(controller) {
  }

  Logger(std::shared_ptr<spdlog::logger> delegate)
        : delegate_(delegate), controller_(nullptr) {
    }


  std::shared_ptr<spdlog::logger> delegate_;
  std::shared_ptr<LoggerControl> controller_;

  std::mutex mutex_;
 private:
  template<typename ... Args>
  inline void log(spdlog::level::level_enum level, const char * const format, const Args& ... args) {
    if (controller_ && !controller_->is_enabled())
         return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!delegate_->should_log(level)) {
      return;
    }
    delegate_->log(level, format_string(format, conditional_conversion(args)...));
  }

  Logger(Logger const&);
  Logger& operator=(Logger const&);
};

#define LOG_DEBUG(x) LogBuilder(x.get(),logging::LOG_LEVEL::debug)

#define LOG_INFO(x) LogBuilder(x.get(),logging::LOG_LEVEL::info)

#define LOG_TRACE(x) LogBuilder(x.get(),logging::LOG_LEVEL::trace)

#define LOG_ERROR(x) LogBuilder(x.get(),logging::LOG_LEVEL::err)

#define LOG_WARN(x) LogBuilder(x.get(),logging::LOG_LEVEL::warn)

} /* namespace logging */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
