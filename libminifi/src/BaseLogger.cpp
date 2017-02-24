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

#include "BaseLogger.h"

// Logger related configuration items.
const char *BaseLogger::nifi_log_level = "nifi.log.level";
const char *BaseLogger::nifi_log_appender = "nifi.log.appender";

/**
 * @brief Log error message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
 void BaseLogger::log_error(const char * const format, ...) {
	if (logger_ == NULL || !logger_->should_log(spdlog::level::level_enum::err))
		return;
	FILL_BUFFER
	log_str(err,buffer);
}
/**
 * @brief Log warn message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
 void BaseLogger::log_warn(const char * const format, ...) {
	if (logger_ == NULL
			|| !logger_->should_log(spdlog::level::level_enum::warn))
		return;
	FILL_BUFFER
	log_str(warn,buffer);
}
/**
 * @brief Log info message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
 void BaseLogger::log_info(const char * const format, ...) {
	if (logger_ == NULL
			|| !logger_->should_log(spdlog::level::level_enum::info))
		return;
	FILL_BUFFER
	log_str(info,buffer);
}
/**
 * @brief Log debug message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
 void BaseLogger::log_debug(const char * const format, ...) {

	if (logger_ == NULL
			|| !logger_->should_log(spdlog::level::level_enum::debug))
		return;
	FILL_BUFFER
	log_str(debug,buffer);
}
/**
 * @brief Log trace message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
 void BaseLogger::log_trace(const char * const format, ...) {

	if (logger_ == NULL
			|| !logger_->should_log(spdlog::level::level_enum::trace))
		return;
	FILL_BUFFER
	log_str(debug,buffer);
}

// overridables

/**
 * @brief Log error message
 * @param format format string ('man printf' for syntax)
 * @warning does not check @p log or @p format for null. Caller must ensure parameters and format string lengths match
 */
void BaseLogger::log_str(LOG_LEVEL_E level, const std::string &buffer) {
	switch (level) {
	case err:
	case critical:
		if (stderr_ != nullptr) {
			stderr_->error(buffer);
		} else {
			logger_->error(buffer);
		}
		break;
	case warn:
		logger_->warn(buffer);
		break;
	case info:
		logger_->info(buffer);
		break;
	case debug:
		logger_->debug(buffer);
		break;
	case trace:
		logger_->trace(buffer);
		break;
	case off:
		break;
	default:
		logger_->info(buffer);
		break;
	}

}

void BaseLogger::setLogLevel(const std::string &level,
		LOG_LEVEL_E defaultLevel) {
	std::string logLevel = level;
	std::transform(logLevel.begin(), logLevel.end(), logLevel.begin(),
			::tolower);

	if (logLevel == "trace") {
		setLogLevel(trace);
	} else if (logLevel == "debug") {
		setLogLevel(debug);
	} else if (logLevel == "info") {
		setLogLevel(info);
	} else if (logLevel == "warn") {
		setLogLevel(warn);
	} else if (logLevel == "error") {
		setLogLevel(err);
	} else if (logLevel == "critical") {
		setLogLevel(critical);
	} else if (logLevel == "off") {
		setLogLevel(off);
	} else {
		setLogLevel(defaultLevel);
	}
}

void BaseLogger::set_error_logger(std::shared_ptr<spdlog::logger> other) {
	stderr_ = std::move(other);
}

