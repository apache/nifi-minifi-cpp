/**
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

#include <sstream>
#include <memory>
#include <string>
#include <utility>
#include <mutex>

#include "spdlog/sinks/sink.h"
#include "spdlog/sinks/ostream_sink.h"

class StringStreamSink : public spdlog::sinks::sink {
 public:
  explicit StringStreamSink(std::shared_ptr<std::ostringstream> stream, std::shared_ptr<std::mutex> log_output_mutex, bool force_flush = false)
      : stream_(std::move(stream)),
        log_output_mutex_(std::move(log_output_mutex)),
        sink_(*stream_, force_flush) {}

  ~StringStreamSink() override = default;

  void log(const spdlog::details::log_msg &msg) override {
    std::lock_guard<std::mutex> guard(*log_output_mutex_);
    sink_.log(msg);
  }
  void flush() override {
    std::lock_guard<std::mutex> guard(*log_output_mutex_);
    sink_.flush();
  }
  void set_pattern(const std::string &pattern) override {
    std::lock_guard<std::mutex> guard(*log_output_mutex_);
    sink_.set_pattern(pattern);
  }
  void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override {
    std::lock_guard<std::mutex> guard(*log_output_mutex_);
    sink_.set_formatter(std::move(sink_formatter));
  }

 private:
  // we need to keep the stream alive as long as the sink is in use,
  // as the sinks are stored in the loggers, some of which are
  // static storage duration, thus they might outlive the provider of
  // the stream
  std::shared_ptr<std::ostringstream> stream_;
  std::shared_ptr<std::mutex> log_output_mutex_;
  spdlog::sinks::ostream_sink_mt sink_;
};
