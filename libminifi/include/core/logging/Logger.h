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
#ifndef LIBMINIFI_INCLUDE_CORE_LOGGING_LOGGER_H_
#define LIBMINIFI_INCLUDE_CORE_LOGGING_LOGGER_H_

#include <string>
#include <mutex>
#include <memory>
#include <sstream>
#include <utility>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string_view>

#include "spdlog/common.h"
#include "spdlog/logger.h"
#include "utils/gsl.h"
#include "utils/SmallString.h"

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

template<size_t N>
inline char const* conditional_conversion(const utils::SmallString<N>& arr) {
  return arr.c_str();
}

template<typename T, typename = typename std::enable_if<
    std::is_arithmetic<T>::value ||
    std::is_enum<T>::value ||
    std::is_pointer<T>::value>::type>
inline T conditional_conversion(T t) {
  return t;
}

template<typename ... Args>
inline std::string format_string(int max_size, char const* format_str, Args&&... args) {
  // try to use static buffer
  char buf[LOG_BUFFER_SIZE + 1];
  int result = std::snprintf(buf, LOG_BUFFER_SIZE + 1, format_str, conditional_conversion(std::forward<Args>(args))...);
  if (result < 0) {
    return std::string("Error while formatting log message");
  }
  if (result <= LOG_BUFFER_SIZE) {
    // static buffer was large enough
    return std::string(buf, gsl::narrow<size_t>(result));
  }
  if (max_size >= 0 && max_size <= LOG_BUFFER_SIZE) {
    // static buffer was already larger than allowed, use the filled buffer
    return std::string(buf, LOG_BUFFER_SIZE);
  }
  // try to use dynamic buffer
  size_t dynamic_buffer_size = max_size < 0 ? gsl::narrow<size_t>(result) : gsl::narrow<size_t>(std::min(result, max_size));
  std::vector<char> buffer(dynamic_buffer_size + 1);  // extra '\0' character
  result = std::snprintf(buffer.data(), buffer.size(), format_str, conditional_conversion(std::forward<Args>(args))...);
  if (result < 0) {
    return std::string("Error while formatting log message");
  }
  return std::string(buffer.cbegin(), buffer.cend() - 1);  // -1 to not include the terminating '\0'
}

inline std::string format_string(int /*max_size*/, char const* format_str) {
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

  void set_max_log_size(int size) {
    max_log_size_ = size;
  }

  bool should_log(const LOG_LEVEL &level);

  virtual void log_string(LOG_LEVEL level, std::string str);

 protected:
  Logger(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller);

  Logger(std::shared_ptr<spdlog::logger> delegate); // NOLINT


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
    const auto str = format_string(max_log_size_.load(), format, conditional_conversion(args)...);
    delegate_->log(level, str);
  }

  std::atomic<int> max_log_size_{LOG_BUFFER_SIZE};

  Logger(Logger const&);
  Logger& operator=(Logger const&);
};

#define LOG_DEBUG(x) LogBuilder(x.get(), logging::LOG_LEVEL::debug)

#define LOG_INFO(x) LogBuilder(x.get(), logging::LOG_LEVEL::info)

#define LOG_TRACE(x) LogBuilder(x.get(), logging::LOG_LEVEL::trace)

#define LOG_ERROR(x) LogBuilder(x.get(), logging::LOG_LEVEL::err)

#define LOG_WARN(x) LogBuilder(x.get(), logging::LOG_LEVEL::warn)

}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_LOGGING_LOGGER_H_
