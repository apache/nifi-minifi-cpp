// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/logging/LoggerBase.h"

#include <mutex>
#include <memory>
#include <iostream>

namespace org::apache::nifi::minifi::core::logging {

LoggerControl::LoggerControl()
    : is_enabled_(true) {
}

bool LoggerControl::is_enabled() const {
  return is_enabled_;
}

void LoggerControl::setEnabled(bool status) {
  is_enabled_ = status;
}

void LoggerBase::setLogCallback(const std::function<void(LOG_LEVEL level, const std::string&)>& callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  log_callback_ = callback;
}

bool LoggerBase::should_log(LOG_LEVEL level) {
  if (controller_ && !controller_->is_enabled())
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  return delegate_->should_log(mapToSpdLogLevel(level));
}

std::string LoggerBase::trimToMaxSizeAndAddId(std::string my_string) {
  auto max_log_size = getMaxLogSize();
  if (max_log_size >= 0 && my_string.size() > gsl::narrow<size_t>(max_log_size))
    my_string = my_string.substr(0, max_log_size);
  if (auto id = get_id()) {
    my_string += *id;
  }
  return my_string;
}

void LoggerBase::log_string(LOG_LEVEL level, std::string str) {
  str = trimToMaxSizeAndAddId(std::move(str));
  if (log_callback_) {
    log_callback_(level, str);
  }
  delegate_->log(mapToSpdLogLevel(level), str.c_str());
}

LOG_LEVEL LoggerBase::level() const {
  return mapFromSpdLogLevel(delegate_->level());
}

LoggerBase::LoggerBase(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller)
    : delegate_(std::move(delegate)), controller_(std::move(controller)) {
}

LoggerBase::LoggerBase(std::shared_ptr<spdlog::logger> delegate)
    : delegate_(std::move(delegate)), controller_(nullptr) {
}

}  // namespace org::apache::nifi::minifi::core::logging
