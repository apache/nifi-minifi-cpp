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

#pragma once

#ifdef WIN32

#include <Windows.h>

#include <string>

#include "spdlog/common.h"
#include "spdlog/details/log_msg.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/sinks/base_sink.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

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

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif
