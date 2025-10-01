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
#include "sitetosite/CompressionOutputStream.h"

#include <algorithm>

#include "io/ZlibStream.h"
#include "io/StreamPipe.h"
#include "io/BufferStream.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::sitetosite {

size_t CompressionOutputStream::write(const uint8_t *value, size_t len) {
  if (value == nullptr || len == 0) {
    return 0;
  }

  const std::span<const std::byte> input_data{reinterpret_cast<const std::byte*>(value), len};
  auto remaining_data = input_data;
  size_t total_bytes_written = 0;

  while (!remaining_data.empty()) {
    const auto free_spaces_left_in_buffer = buffer_.size() - buffer_offset_;
    const auto bytes_to_write = std::min(remaining_data.size(), free_spaces_left_in_buffer);

    const auto chunk_to_write = remaining_data.subspan(0, bytes_to_write);
    const auto buffer_destination = std::span<std::byte>(buffer_).subspan(buffer_offset_, bytes_to_write);

    std::ranges::copy(chunk_to_write, buffer_destination.begin());

    total_bytes_written += bytes_to_write;
    remaining_data = remaining_data.subspan(bytes_to_write);
    buffer_offset_ += bytes_to_write;

    gsl_Assert(buffer_offset_ <= buffer_.size());

    if (buffer_offset_ == buffer_.size()) {
      auto ret = compressAndWrite();
      if (io::isError(ret)) {
        return ret;
      }
    }
  }

  return total_bytes_written;
}

size_t CompressionOutputStream::compressAndWrite() {
  if (was_data_written_) {
    // Write a continue byte to indicate that there is more data to follow
    auto ret = internal_stream_.write(static_cast<uint8_t>(1));
    if (io::isError(ret)) {
      logger_->log_error("Failed to write continue byte before compression: {}", ret);
      return ret;
    }
  }
  auto ret = internal_stream_.write(reinterpret_cast<const uint8_t *>(SYNC_BYTES.data()), SYNC_BYTES.size());
  if (io::isError(ret)) {
    logger_->log_error("Failed to write sync bytes before compression: {}", ret);
    return ret;
  }

  io::BufferStream buffer_stream;
  {
    io::ZlibCompressStream zlib_stream{gsl::make_not_null(&buffer_stream), io::ZlibCompressionFormat::ZLIB, Z_BEST_SPEED};
    ret = zlib_stream.write(gsl::make_span(buffer_).subspan(0, buffer_offset_));
    if (io::isError(ret)) {
      logger_->log_error("Failed to write data to zlib stream: {}", ret);
      return ret;
    }
    gsl_Assert(buffer_offset_ == ret);
    zlib_stream.close();
    gsl_Assert(zlib_stream.isFinished());
  }

  // Write the original size of the data before compression
  ret = internal_stream_.write(gsl::narrow<uint32_t>(buffer_offset_));
  if (io::isError(ret)) {
    logger_->log_error("Failed to write original size before compression: {}", ret);
    return ret;
  }

  // Write the compressed size of the data
  ret = internal_stream_.write(gsl::narrow<uint32_t>(buffer_stream.size()));
  if (io::isError(ret)) {
    return ret;
  }

  // Write the compressed data
  ret = internal::pipe(buffer_stream, internal_stream_);
  if (io::isError(ret)) {
    return ret;
  }

  buffer_offset_ = 0;
  was_data_written_ = true;
  return ret;
}

void CompressionOutputStream::flush() {
  if (buffer_offset_ > 0) {
    auto ret = compressAndWrite();
    if (io::isError(ret)) {
      logger_->log_error("Flush failed when compressing data: {}", ret);
      return;
    }
  }
  if (was_data_written_) {
    was_data_written_ = false;
    auto ret = internal_stream_.write(static_cast<uint8_t>(0));
    if (io::isError(ret)) {
      logger_->log_error("Flush failed when writing on internal stream: {}", ret);
      return;
    }
  }
}

void CompressionOutputStream::close() {
  flush();
  internal_stream_.close();
}

}  // namespace org::apache::nifi::minifi::sitetosite
