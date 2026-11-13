// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#ifdef WIN32

#include <Windows.h>

#include <string>

#include "spdlog/common.h"
#include "spdlog/details/log_msg.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/sinks/base_sink.h"

namespace org::apache::nifi::minifi::core::logging::internal {

class windowseventlog_sink : public spdlog::sinks::base_sink<spdlog::details::null_mutex> {
 private:
  HANDLE event_source_;

  WORD type_from_level(const spdlog::details::log_msg& msg) const;

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) final;

  void flush_() final;

 public:
  windowseventlog_sink(const std::string& source_name = "ApacheNiFiMiNiFi"); // NOLINT

  virtual ~windowseventlog_sink();

  windowseventlog_sink(const windowseventlog_sink&) = delete;
  windowseventlog_sink& operator=(const windowseventlog_sink&) = delete;
  windowseventlog_sink(windowseventlog_sink&&) = delete;
  windowseventlog_sink& operator=(windowseventlog_sink&&) = delete;
};

}  // namespace org::apache::nifi::minifi::core::logging::internal

#endif
