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
#ifndef LIBMINIFI_INCLUDE_LOGAPPENDERS_H_
#define LIBMINIFI_INCLUDE_LOGAPPENDERS_H_

#include "BaseLogger.h"
#include "spdlog/sinks/null_sink.h"
#include "spdlog/sinks/ostream_sink.h"
#include <cxxabi.h>
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

template<typename T>
static std::string getUniqueName() {
  std::string name = LOG_NAME;
  name += " -- ";
  name += abi::__cxa_demangle(typeid(T).name(), 0, 0, 0);
  spdlog::drop(name);
  return name;
}

/**
 * Null appender sets a null sink, thereby performing no logging.
 */
class NullAppender : public BaseLogger {
 public:
  /**
   * Base constructor that creates the null sink.
   */
  explicit NullAppender()
      : BaseLogger("off") {
    auto null_sink = std::make_shared<spdlog::sinks::null_sink_st>();
    std::string unique_name = getUniqueName<NullAppender>();
    logger_ = std::make_shared<spdlog::logger>(unique_name, null_sink);
    configured_level_ = off;
    setLogLevel();
  }

  /**
   * Move constructor for the null appender.
   */
  explicit NullAppender(const NullAppender &&other)
      : BaseLogger(std::move(other)) {

  }

};

/**
 * Basic output stream configuration that uses a supplied ostream
 *
 * Design : extends LoggerConfiguration using the logger and log level
 * encapsulated within the base configuration class.
 */
class OutputStreamAppender : public BaseLogger {

 public:

  static const char *nifi_log_output_stream_error_stderr;

  /**
   * Output stream move constructor.
   */
  explicit OutputStreamAppender(const OutputStreamAppender &&other)
      : BaseLogger(std::move(other)) {

  }

  /**
   * Base constructor. Creates a ostream sink.
   * @param stream incoming stream reference.
   * @param config configuration.
   */
  explicit OutputStreamAppender(std::shared_ptr<Configure> config)
      : BaseLogger("info") {
    auto ostream_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(
        std::cout);

    std::string unique_name = getUniqueName<OutputStreamAppender>();
    logger_ = std::make_shared<spdlog::logger>(unique_name, ostream_sink);

    std::string use_std_err;

    if (NULL != config
        && config->get(nifi_log_output_stream_error_stderr, use_std_err)) {

      std::transform(use_std_err.begin(), use_std_err.end(),
                     use_std_err.begin(), ::tolower);

      if (use_std_err == "true") {
        std::string err_unique_name = getUniqueName<OutputStreamAppender>();
        auto error_ostream_sink = std::make_shared<
            spdlog::sinks::ostream_sink_mt>(std::cerr);
        stderr_ = std::make_shared<spdlog::logger>(err_unique_name,
                                                   error_ostream_sink);
      }
    } else {
      stderr_ = nullptr;
    }

    std::string log_level;
    if (NULL != config && config->get(BaseLogger::nifi_log_level, log_level)) {
      setLogLevel(log_level);
    } else {
      setLogLevel("info");
    }

  }

  /**
   * Base constructor. Creates a ostream sink.
   * @param stream incoming stream reference.
   * @param config configuration.
   */
  OutputStreamAppender(std::ostream &stream, std::shared_ptr<Configure> config)
      : BaseLogger("info") {
    auto ostream_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(
        stream);
    std::string unique_name = getUniqueName<OutputStreamAppender>();
    logger_ = std::make_shared<spdlog::logger>(unique_name, ostream_sink);

    stderr_ = nullptr;

    std::string log_level;
    if (NULL != config && config->get(BaseLogger::nifi_log_level, log_level)) {
      setLogLevel(log_level);
    } else {
      setLogLevel("info");
    }

  }

 protected:

};

/**
 * Rolling configuration
 * Design : extends LoggerConfiguration using the logger and log level
 * encapsulated within the base configuration class.
 */
class RollingAppender : public BaseLogger {
 public:
  static const char *nifi_log_rolling_apender_file;
  static const char *nifi_log_rolling_appender_max_files;
  static const char *nifi_log_rolling_appender_max_file_size;

  /**
   * RollingAppenderConfiguration move constructor.
   */
  explicit RollingAppender(const RollingAppender&& other)
      : BaseLogger(std::move(other)),
        max_files_(std::move(other.max_files_)),
        file_name_(std::move(other.file_name_)),
        max_file_size_(std::move(other.max_file_size_)) {
  }
  /**
   * Base Constructor.
   * @param config pointer to the configuration for this instance.
   */
  explicit RollingAppender(std::shared_ptr<Configure> config = 0)
      : BaseLogger("info") {
    std::string file_name = "";
    if (NULL != config
        && config->get(nifi_log_rolling_apender_file, file_name)) {
      file_name_ = file_name;
    } else {
      file_name_ = LOG_FILE_NAME;
    }

    std::string max_files = "";
    if (NULL != config
        && config->get(nifi_log_rolling_appender_max_files, max_files)) {
      try {
        max_files_ = std::stoi(max_files);
      } catch (const std::invalid_argument &ia) {
        max_files_ = DEFAULT_LOG_FILE_NUMBER;
      } catch (const std::out_of_range &oor) {
        max_files_ = DEFAULT_LOG_FILE_NUMBER;
      }
    } else {
      max_files_ = DEFAULT_LOG_FILE_NUMBER;
    }

    std::string max_file_size = "";
    if (NULL != config
        && config->get(nifi_log_rolling_appender_max_file_size,
                       max_file_size)) {
      try {
        max_file_size_ = std::stoi(max_file_size);
      } catch (const std::invalid_argument &ia) {
        max_file_size_ = DEFAULT_LOG_FILE_SIZE;
      } catch (const std::out_of_range &oor) {
        max_file_size_ = DEFAULT_LOG_FILE_SIZE;
      }
    } else {
      max_file_size_ = DEFAULT_LOG_FILE_SIZE;
    }

    std::string unique_name = getUniqueName<OutputStreamAppender>();
    logger_ = spdlog::rotating_logger_mt(unique_name, file_name_,
                                         max_file_size_, max_files_);

    std::string log_level;
    if (NULL != config && config->get(BaseLogger::nifi_log_level, log_level)) {
      setLogLevel(log_level);
    }
  }

  /**
   * To maintain current functionality we will flush on write.
   */
  void log_str(LOG_LEVEL_E level, const std::string &buffer) {
    BaseLogger::log_str(level, buffer);
    logger_->flush();
  }

 protected:

  /**
   * file name.
   */
  std::string file_name_;
  /**
   * maximum number of files to keep in the rotation.
   */
  size_t max_files_;
  /**
   * Maximum file size per rotated file.
   */
  size_t max_file_size_;

};

class LogInstance {
 public:
  /**
   * Returns a logger configuration based on
   * the configuration within this instance.
   * @param config configuration for this instance.
   */
  static std::unique_ptr<BaseLogger> getConfiguredLogger(std::shared_ptr<Configure> config) {
    std::string appender = "";

    if (config->get(BaseLogger::nifi_log_appender, appender)) {
      std::transform(appender.begin(), appender.end(), appender.begin(),
                     ::tolower);

      if ("nullappender" == appender || "null appender" == appender
          || "null" == appender) {

        return std::move(std::unique_ptr<BaseLogger>(new NullAppender()));

      } else if ("rollingappender" == appender || "rolling appender" == appender
          || "rolling" == appender) {

        return std::move(
            std::unique_ptr<BaseLogger>(new RollingAppender(config)));

      } else if ("outputstream" == appender
          || "outputstreamappender" == appender
          || "outputstream appender" == appender) {

        return std::move(
            std::unique_ptr<BaseLogger>(new OutputStreamAppender(config)));

      }
    }
    return nullptr;

  }
};

} /* namespace logging */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_LOGAPPENDERS_H_ */
