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

#include <cinttypes>
#include <functional>
#include <memory>
#include <optional>

#include "../../minifi-api/include/minifi-c/minifi-c.h"
#include "Stream.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::io {

class InputStream;
class OutputStream;

class IoResult {
 public:
  IoResult() = delete;
  IoResult(const IoResult&) = default;
  IoResult(IoResult&&) noexcept = default;
  IoResult& operator=(IoResult&&) noexcept = default;
  IoResult& operator=(const IoResult&) = default;

  virtual ~IoResult() = default;

  static IoResult error() { return IoResult(nonstd::make_unexpected(MINIFI_IO_ERROR)); }
  static IoResult cancelled() { return IoResult(nonstd::make_unexpected(MINIFI_IO_CANCEL)); }
  static IoResult zero() { return IoResult(0U); }

  static IoResult fromI64(int64_t i64_val) {
    if (i64_val < 0) { return IoResult(nonstd::make_unexpected(static_cast<MinifiIoStatus>(i64_val))); }
    return IoResult(gsl::narrow<uint64_t>(i64_val));
  }

  static IoResult fromSizeT(const size_t val) {
    if (isError(val)) { return IoResult::error(); }
    return IoResult(gsl::narrow<uint64_t>(val));
  }

  [[nodiscard]] int64_t toI64() const {
    if (result_.has_value()) { return gsl::narrow<int64_t>(*result_); }
    return result_.error();
  }

  [[nodiscard]] bool is_cancelled() const { return !result_ && result_.error() == MINIFI_IO_CANCEL; }

  bool operator()() const { return result_.has_value(); }
  bool operator!() const { return !result_.has_value(); }

  uint64_t operator*() const { return *result_; }

  nonstd::expected<uint64_t, MinifiIoStatus> inner() const { return result_; }

 private:
  explicit IoResult(nonstd::expected<uint64_t, MinifiIoStatus> result) : result_(std::move(result)) {}

  nonstd::expected<uint64_t, MinifiIoStatus> result_;
};

class ReadWriteResult {
 public:
  ReadWriteResult() = delete;
  ReadWriteResult(const ReadWriteResult&) = default;
  ReadWriteResult(ReadWriteResult&&) noexcept = default;
  ReadWriteResult& operator=(ReadWriteResult&&) noexcept = default;
  ReadWriteResult& operator=(const ReadWriteResult&) = default;

  ReadWriteResult(const uint64_t bytes_read, const uint64_t bytes_written)
      : result_(ReadWrite{.bytes_read = bytes_read, .bytes_written = bytes_written}) {}

  static ReadWriteResult zero() { return ReadWriteResult(ReadWrite{.bytes_read = 0, .bytes_written = 0}); };
  static ReadWriteResult error() { return ReadWriteResult(nonstd::make_unexpected(MINIFI_IO_ERROR)); }
  static ReadWriteResult cancelled() { return ReadWriteResult(nonstd::make_unexpected(MINIFI_IO_CANCEL)); }

  virtual ~ReadWriteResult() = default;

  bool operator()() const { return result_.has_value(); }
  bool operator!() const { return !result_.has_value(); }

  [[nodiscard]] uint64_t bytesWritten() const { return result_->bytes_written; }
  [[nodiscard]] uint64_t bytesRead() const { return result_->bytes_read; }

 private:
  struct ReadWrite {
    uint64_t bytes_read;
    uint64_t bytes_written;
  };
  explicit ReadWriteResult(nonstd::expected<ReadWrite, MinifiIoStatus> result) : result_(std::move(result)) {}

  nonstd::expected<ReadWrite, MinifiIoStatus> result_;
};

using OutputStreamCallback = std::function<IoResult(const std::shared_ptr<OutputStream>& output_stream)>;
using InputStreamCallback = std::function<IoResult(const std::shared_ptr<InputStream>& input_stream)>;
using InputOutputStreamCallback =
    std::function<ReadWriteResult(const std::shared_ptr<InputStream>& input_stream, const std::shared_ptr<OutputStream>& output_stream)>;

}  // namespace org::apache::nifi::minifi::io
