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

#include "spdlog/common.h"
#include "spdlog/logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

#define LOG_BUFFER_SIZE 1024

class LoggerControl {
 public:
  LoggerControl();

  bool is_enabled() const;

  void setEnabled(bool status);

 protected:
  std::atomic<bool> is_enabled_;
};

inline char const* conditional_conversion(std::string const& str) {
  return str.c_str();
}

template<typename T>
inline T conditional_conversion(T const& t) {
  return t;
}

template<typename ... Args>
inline std::string format_string(char const* format_str, Args&&... args) {
  char buf[LOG_BUFFER_SIZE];
  std::snprintf(buf, LOG_BUFFER_SIZE, format_str, conditional_conversion(std::forward<Args>(args))...);
  return std::string(buf);
}

inline std::string format_string(char const* format_str) {
  return format_str;
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

  virtual ~BaseLogger();

  virtual void log_string(LOG_LEVEL level, std::string str) = 0;

  virtual bool should_log(const LOG_LEVEL &level);

};

/**
 * LogBuilder is a class to facilitate using the LOG macros below and an associated put-to operator.
 *
 */
class LogBuilder {
 public:
  LogBuilder(BaseLogger *l, LOG_LEVEL level);

  ~LogBuilder();

  void setIgnore();

  void log_string(LOG_LEVEL level);

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

  bool should_log(const LOG_LEVEL &level);

 protected:

  virtual void log_string(LOG_LEVEL level, std::string str);

  Logger(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller);

  Logger(std::shared_ptr<spdlog::logger> delegate);


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
    const auto str = format_string(format, conditional_conversion(args)...);
    delegate_->log(level, str);
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
