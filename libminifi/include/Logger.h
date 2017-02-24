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
#include <atomic>
#include <memory>
#include <utility>
#include <algorithm>
#include <cstdio>
#include <iostream>

#include "BaseLogger.h"
#include "spdlog/spdlog.h"

/**
 * Logger class
 * Design: Extends BaseLogger, leaving this class to be the facade to the underlying
 * logging mechanism. Is a facade to BaseLogger's underlying log stream. This allows
 * the underlying implementation to be replaced real time.
 */
class Logger: public BaseLogger {
protected:
	struct singleton;
public:

	/**
	 * Returns a shared pointer to the logger instance.
	 * Note that while there is no synchronization this is expected
	 * to be called and initialized first
	 * @returns shared pointer to the base logger.
	 */
	static std::shared_ptr<Logger> getLogger() {

		if (singleton_logger_ == nullptr)
			singleton_logger_ = std::make_shared<Logger>(singleton { 0 });
		return singleton_logger_;
	}

	/**
	 * Returns the log level for this instance.
	 */
	LOG_LEVEL_E getLogLevel() const {
		return current_logger_.load()->getLogLevel();
	}

	/**
	 * Sets the log level atomic and sets it
	 * within logger if it can
	 * @param level desired log level.
	 */
	void setLogLevel(LOG_LEVEL_E level) {
		current_logger_.load()->setLogLevel(level);
	}

	/**
	 * Sets the log level for this instance based on the string
	 * @param level desired log leve.
	 * @param defaultLevel default level if we cannot match level.
	 */
	void setLogLevel(const std::string &level,
			LOG_LEVEL_E defaultLevel = info) {
		current_logger_.load()->setLogLevel(level, info);
	}

	void updateLogger(std::unique_ptr<BaseLogger> logger) {

		if (logger == nullptr  )
			return;
		current_logger_.store(logger.release());
	}

	/**
	 * @brief Log error message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_error(const char * const format, ...) {
		if (!current_logger_.load()->shouldLog(err))
			return;
		FILL_BUFFER
		current_logger_.load()->log_str(err, buffer);
	}
	/**
	 * @brief Log warn message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_warn(const char * const format, ...) {
		if (!current_logger_.load()->shouldLog(warn))
			return;
		FILL_BUFFER
		current_logger_.load()->log_str(warn, buffer);
	}
	/**
	 * @brief Log info message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_info(const char * const format, ...) {
		if (!current_logger_.load()->shouldLog(info))
			return;
		FILL_BUFFER
		current_logger_.load()->log_str(info, buffer);
	}
	/**
	 * @brief Log debug message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_debug(const char * const format, ...) {

		if (!current_logger_.load()->shouldLog(debug))
			return;
		FILL_BUFFER
		current_logger_.load()->log_str(debug, buffer);
	}
	/**
	 * @brief Log trace message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	void log_trace(const char * const format, ...) {

		if (!current_logger_.load()->shouldLog(trace))
			return;
		FILL_BUFFER
		current_logger_.load()->log_str(trace, buffer);
	}

	/**
	 * @brief Log message
	 * @param format format string ('man printf' for syntax)
	 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
	 */
	virtual void log_str(LOG_LEVEL_E level, const std::string &buffer) {
		current_logger_.load()->log_str(level, buffer);
	}

	//! Destructor
	~Logger() {
	}

	explicit Logger(const singleton &a) {

		/**
		 * flush on info to maintain current functionality
		 */
		std::shared_ptr<spdlog::logger> defaultsink = spdlog::rotating_logger_mt(LOG_NAME,
				LOG_FILE_NAME,
				DEFAULT_LOG_FILE_SIZE, DEFAULT_LOG_FILE_NUMBER);
		defaultsink->flush_on(spdlog::level::level_enum::info);

		std::unique_ptr<BaseLogger> new_logger_ = std::unique_ptr<BaseLogger>(
				new BaseLogger("info", defaultsink));

		new_logger_->setLogLevel(info);
		current_logger_.store(new_logger_.release());
	}

	Logger(const Logger &parent) = delete;
	Logger &operator=(const Logger &parent) = delete;

protected:

	/**
	 * Allows for a null constructor above so that we can have a public constructor that
	 * effectively limits us to being a singleton by having a protected argument in the constructor
	 */
	struct singleton {
		explicit singleton(int) {
		}
	};

	std::atomic<BaseLogger*> current_logger_;

//! Singleton logger instance
	static std::shared_ptr<Logger> singleton_logger_;
};

#endif
