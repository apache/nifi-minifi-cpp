/**
 * @file Logger.h
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
#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <string>
#include <algorithm>
#include <cstdio>
#include "spdlog/spdlog.h"

using spdlog::stdout_logger_mt;
using spdlog::rotating_logger_mt;
using spdlog::logger;

#define LOG_BUFFER_SIZE 1024
#define FILL_BUFFER  char buffer[LOG_BUFFER_SIZE]; \
    va_list args; \
    va_start(args, format); \
    std::vsnprintf(buffer, LOG_BUFFER_SIZE,format, args); \
    va_end(args);

//! 5M default log file size
#define DEFAULT_LOG_FILE_SIZE (5*1024*1024)
//! 3 log files rotation
#define DEFAULT_LOG_FILE_NUMBER 3
#define LOG_NAME "minifi log"
#define LOG_FILE_NAME "minifi-app.log"

typedef enum
{
    trace    = 0,
    debug    = 1,
    info     = 2,
    notice   = 3,
    warn     = 4,
    err      = 5,
    critical = 6,
    alert    = 7,
    emerg    = 8,
    off      = 9
} LOG_LEVEL_E;

//! Logger Class
class Logger {

public:

	//! Get the singleton logger instance
	static Logger * getLogger() {
		if (!logger_)
			logger_ = new Logger();
		return logger_;
	}
	void setLogLevel(LOG_LEVEL_E level) {
		if (spdlog_ == NULL)
			return;
		spdlog_->set_level((spdlog::level::level_enum) level);
	}

	void setLogLevel(const std::string &level,LOG_LEVEL_E defaultLevel = info )
	{
		std::string logLevel = "";
		std::transform(level.begin(), level.end(), logLevel.end(), ::tolower);

		if (logLevel == "trace") {
			setLogLevel(trace);
		} else if (logLevel == "debug") {
			setLogLevel(debug);
		} else if (logLevel == "info") {
			setLogLevel(info);
		} else if (logLevel == "notice") {
			setLogLevel(notice);
		} else if (logLevel == "warn") {
			setLogLevel(warn);
		} else if (logLevel == "error") {
			setLogLevel(err);
		} else if (logLevel == "critical") {
			setLogLevel(critical);
		} else if (logLevel == "alert") {
			setLogLevel(alert);
		} else if (logLevel == "emerg") {
			setLogLevel(emerg);
		} else if (logLevel == "off") {
			setLogLevel(off);
		} else {
			setLogLevel(defaultLevel);
		}
	}
	//! Destructor
	~Logger() {}
	/**
	 * @brief Log error message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_error(const char *const format, ...) {
		if(spdlog_ == NULL || !spdlog_->should_log(spdlog::level::level_enum::err))
			return;
		FILL_BUFFER
	    spdlog_->error(buffer);
	}
	/**
	 * @brief Log warn message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_warn(const char *const format, ...) {
		if(spdlog_ == NULL || !spdlog_->should_log(spdlog::level::level_enum::warn))
			return;
		FILL_BUFFER
	    spdlog_->warn(buffer);
	}
	/**
	 * @brief Log info message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_info(const char *const format, ...) {
		if(spdlog_ == NULL || !spdlog_->should_log(spdlog::level::level_enum::info))
			return;
		FILL_BUFFER
	    spdlog_->info(buffer);
	}
	/**
	 * @brief Log debug message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_debug(const char *const format, ...) {
		if(spdlog_ == NULL || !spdlog_->should_log(spdlog::level::level_enum::debug))
			return;
		FILL_BUFFER
	    spdlog_->debug(buffer);
	}
	/**
	 * @brief Log trace message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_trace(const char *const format, ...) {
		if(spdlog_ == NULL || !spdlog_->should_log(spdlog::level::level_enum::trace))
			return;
		FILL_BUFFER
	    spdlog_->trace(buffer);
	}

protected:

private:
	// Prevent default copy constructor and assignment operation
	// Only support pass by reference or pointer
	Logger(const Logger &parent);
	Logger &operator=(const Logger &parent);
	//! Constructor
	/*!
	 * Create a logger
	 * */
	Logger(const std::string logger_name = LOG_NAME, const std::string filename = LOG_FILE_NAME, size_t max_file_size = DEFAULT_LOG_FILE_SIZE, size_t max_files = DEFAULT_LOG_FILE_NUMBER, bool force_flush = true) {
		spdlog_ = rotating_logger_mt(logger_name, filename, max_file_size, max_files);
		spdlog_->set_level((spdlog::level::level_enum) debug);
	}
	//! spdlog
	std::shared_ptr<logger> spdlog_;

	//! Singleton logger instance
	static Logger *logger_;
};

#endif
