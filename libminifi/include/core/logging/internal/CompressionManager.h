// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <utility>
#include <string>
#include <vector>

#include "minifi-cpp/core/logging/Logger.h"
#include "LogCompressorSink.h"
#include "core/logging/LoggerProperties.h"
#include "minifi-cpp/io/InputStream.h"
#include "minifi-cpp/utils/Literals.h"

class LoggerTestAccessor;

namespace org::apache::nifi::minifi::core::logging::internal {

class CompressionManager {
  friend class ::LoggerTestAccessor;

  using LoggerFactory = std::function<std::shared_ptr<Logger>(const std::string&)>;

 public:
  std::shared_ptr<LogCompressorSink> initialize(const std::shared_ptr<LoggerProperties>& properties, const std::shared_ptr<Logger>& error_logger, const LoggerFactory& logger_factory);

  template<class Rep, class Period>
  std::vector<std::unique_ptr<io::InputStream>> getCompressedLogs(const std::chrono::duration<Rep, Period>& time) {
    std::shared_ptr<internal::LogCompressorSink> sink = getSink();
    if (sink) {
      return sink->getContent(time);
    }
    return {};
  }

  static constexpr const char* compression_cached_log_max_size_ = "compression.cached.log.max.size";
  static constexpr const char* compression_compressed_log_max_size_ = "compression.compressed.log.max.size";

 private:
  std::shared_ptr<internal::LogCompressorSink> getSink() const {
    // gcc4.8 bug => cannot use std::atomic_load
    std::lock_guard<std::mutex> lock(mtx_);
    return sink_;
  }

  std::atomic<size_t> cache_segment_size{1_MiB};
  std::atomic<size_t> compressed_segment_size{1_MiB};

  mutable std::mutex mtx_;
  std::shared_ptr<LogCompressorSink> sink_;
};

}  // namespace org::apache::nifi::minifi::core::logging::internal
