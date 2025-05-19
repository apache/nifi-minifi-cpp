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
#include "sitetosite/CompressionInputStream.h"
#include "io/ZlibStream.h"

namespace org::apache::nifi::minifi::sitetosite {

size_t CompressionInputStream::decompressData() {
  if (eof_) {
    return 0;
  }

  std::vector<std::byte> local_buffer(COMPRESSION_BUFFER_SIZE);
  auto ret = internal_stream_->read(std::span(local_buffer).subspan(0, SYNC_BYTES.size()));
  if (ret != SYNC_BYTES.size() ||
      !std::equal(SYNC_BYTES.begin(), SYNC_BYTES.end(), local_buffer.begin(), [](char sync_char, std::byte read_byte) { return static_cast<std::byte>(sync_char) == read_byte;})) {
    logger_->log_error("Failed to read sync bytes or sync bytes do not match");
    return io::STREAM_ERROR;
  }

  uint32_t original_size = 0;
  ret = internal_stream_->read(original_size);
  if (io::isError(ret) || ret != 4) {
    logger_->log_error("Failed to read original size, ret: {}", ret);
    return io::STREAM_ERROR;
  }

  uint32_t compressed_size = 0;
  ret = internal_stream_->read(compressed_size);
  if (io::isError(ret) || ret != 4) {
    logger_->log_error("Failed to read compressed size, ret: {}", ret);
    return io::STREAM_ERROR;
  }

  ret = internal_stream_->read(std::span(local_buffer).subspan(0, compressed_size));
  if (io::isError(ret) || ret != compressed_size) {
    logger_->log_error("Failed to read compressed data, ret: {}", ret);
    return io::STREAM_ERROR;
  }

  if (compressed_size == 0 && original_size != 0) {
    logger_->log_error("Compressed size is 0 but original size is not");
    return io::STREAM_ERROR;
  }

  if (compressed_size > COMPRESSION_BUFFER_SIZE) {
    logger_->log_error("Compressed size exceeds buffer size");
    return io::STREAM_ERROR;
  }

  if (original_size > COMPRESSION_BUFFER_SIZE) {
    logger_->log_error("Original size exceeds buffer size");
    return io::STREAM_ERROR;
  }

  if (compressed_size != 0) {
    io::BufferStream decompressed_data_stream;
    io::ZlibDecompressStream zlib_stream{gsl::make_not_null(&decompressed_data_stream), io::ZlibCompressionFormat::ZLIB};
    ret = zlib_stream.write(std::span(local_buffer).subspan(0, compressed_size));
    if (io::isError(ret)) {
      logger_->log_error("Failed to write compressed data to zlib stream, ret: {}", ret);
      return ret;
    }
    zlib_stream.close();
    gsl_Assert(zlib_stream.isFinished());

    ret = decompressed_data_stream.read(std::span(buffer_).subspan(0, original_size));
    if (io::isError(ret) || ret != original_size) {
      logger_->log_error("Failed to read decompressed data, ret: {}", ret);
      return io::STREAM_ERROR;
    }
  }

  uint8_t end_byte = 0;
  ret = internal_stream_->read(end_byte);
  if (io::isError(ret) || ret != 1) {
    logger_->log_error("Failed to read end byte, ret: {}", ret);
    return io::STREAM_ERROR;
  }

  // If end_byte is 0, it indicates EOF, if it is 1, it indicates more data will follow
  if (end_byte == 0) {
    eof_ = true;
  } else if (end_byte != 1) {
    logger_->log_error("End byte is not 0 or 1, received: {}", end_byte);
    return io::STREAM_ERROR;
  }

  buffered_data_length_ = original_size;
  buffer_offset_ = 0;
  return original_size;
}

size_t CompressionInputStream::read(std::span<std::byte> out_buffer) {
  if (eof_ && buffered_data_length_ == 0) {
    return 0;
  }

  size_t bytes_to_read = out_buffer.size();
  size_t bytes_read = 0;
  while (bytes_to_read > 0) {
    if (buffered_data_length_ == 0 || buffered_data_length_ == buffer_offset_) {
      auto ret = decompressData();
      if (io::isError(ret)) {
        return io::STREAM_ERROR;
      }
    }
    uint64_t bytes_available = buffered_data_length_ - buffer_offset_;
    if (bytes_available == 0) {
      break;
    }
    if (bytes_available <= bytes_to_read) {
      std::memcpy(out_buffer.data(), buffer_.data() + buffer_offset_, bytes_available);
      buffer_offset_ = 0;
      buffered_data_length_ = 0;
      bytes_to_read -= bytes_available;
      bytes_read += bytes_available;
    } else {
      std::memcpy(out_buffer.data(), buffer_.data() + buffer_offset_, bytes_to_read);
      buffer_offset_ += bytes_to_read;
      bytes_read += bytes_to_read;
      bytes_to_read = 0;
    }
  }

  return bytes_read;
}

void CompressionInputStream::close() {
  internal_stream_->close();
}

}  // namespace org::apache::nifi::minifi::sitetosite
