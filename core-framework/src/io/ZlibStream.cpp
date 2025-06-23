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

#include "io/ZlibStream.h"
#include "Exception.h"
#include "utils/gsl.h"
#include "core/logging/LoggerFactory.h"
#include "magic_enum.hpp"

namespace org::apache::nifi::minifi::io {

ZlibBaseStream::ZlibBaseStream(gsl::not_null<OutputStream*> output)
    : outputBuffer_(16384U),
      output_{output} {
  strm_.zalloc = Z_NULL;
  strm_.zfree = Z_NULL;
  strm_.opaque = Z_NULL;
}

bool ZlibBaseStream::isFinished() const {
  return state_ == ZlibStreamState::FINISHED;
}

ZlibCompressStream::ZlibCompressStream(gsl::not_null<OutputStream*> output, ZlibCompressionFormat format, int level)
  : ZlibCompressStream(output, format, level, core::logging::LoggerFactory<ZlibCompressStream>::getLogger()) {}

ZlibCompressStream::ZlibCompressStream(gsl::not_null<OutputStream*> output, ZlibCompressionFormat format, int level, std::shared_ptr<core::logging::Logger> logger)
  : ZlibBaseStream(output),
    logger_{std::move(logger)} {
  int ret = deflateInit2(
      &strm_,
      level,
      Z_DEFLATED /* method */,
      15 + (format == ZlibCompressionFormat::GZIP ? 16 : 0) /* windowBits */,
      8 /* memLevel */,
      Z_DEFAULT_STRATEGY /* strategy */);
  if (ret != Z_OK) {
    logger_->log_error("Failed to initialize z_stream with deflateInit2, error code: {}", ret);
    throw Exception(ExceptionType::GENERAL_EXCEPTION, "zlib deflateInit2 failed");
  }

  state_ = ZlibStreamState::INITIALIZED;
}

ZlibCompressStream::~ZlibCompressStream() {
  if (state_ != ZlibStreamState::UNINITIALIZED) {
    int result = deflateEnd(&strm_);
    if (result == Z_DATA_ERROR) {
      logger_->log_debug("Stream was freed prematurely");
    } else if (result == Z_STREAM_ERROR) {
      logger_->log_debug("Stream state was inconsistent");
    } else if (result != Z_OK) {
      logger_->log_debug("Unknown error while finishing compression {}", result);
    }
  }
}

size_t ZlibCompressStream::write(const uint8_t *value, size_t size) {
  return write(value, size, Z_NO_FLUSH);
}

size_t ZlibCompressStream::write(const uint8_t* value, size_t size, FlushMode mode) {
  if (state_ != ZlibStreamState::INITIALIZED) {
    logger_->log_error("writeData called in invalid ZlibCompressStream state, state is {}", magic_enum::enum_name(state_));
    return STREAM_ERROR;
  }

  strm_.next_in = const_cast<uint8_t*>(value);
  strm_.avail_in = gsl::narrow<uInt>(size);

  /*
   * deflate consumes all input data it can (i.e. if it has enough output buffer it never leaves input data unconsumed)
   * and fills the output buffer to the brim every time it can. This means that the proper way to use deflate is to
   * watch avail_out: once deflate does not have to fill it to the brim, then it has consumed all data we have provided
   * to it, and does not need more output buffer for the time being.
   * When we have no more input data, we must call deflate with Z_FINISH to make it empty its internal buffers and
   * close the compressed stream.
   */
  do {
    logger_->log_trace("writeData has {} B of input data left", strm_.avail_in);

    strm_.next_out = reinterpret_cast<Bytef*>(outputBuffer_.data());
    strm_.avail_out = gsl::narrow<uInt>(outputBuffer_.size());

    logger_->log_trace("calling deflate with flush {}", mode);

    int ret = deflate(&strm_, mode);
    if (ret == Z_STREAM_ERROR) {
      logger_->log_error("deflate failed, error code: {}", ret);
      state_ = ZlibStreamState::ERRORED;
      return STREAM_ERROR;
    }
    const auto output_size = outputBuffer_.size() - strm_.avail_out;
    logger_->log_trace("deflate produced {} B of output data", output_size);
    if (output_->write(gsl::make_span(outputBuffer_).subspan(0, output_size)) != output_size) {
      logger_->log_error("Failed to write to underlying stream");
      state_ = ZlibStreamState::ERRORED;
      return STREAM_ERROR;
    }
  } while (strm_.avail_out == 0);

  return size;
}

void ZlibCompressStream::close() {
  if (state_ == ZlibStreamState::INITIALIZED) {
    if (write(nullptr, 0U, Z_FINISH) == 0) {
      state_ = ZlibStreamState::FINISHED;
    }
  }
}

ZlibDecompressStream::ZlibDecompressStream(gsl::not_null<OutputStream*> output, ZlibCompressionFormat format)
    : ZlibBaseStream(output),
      logger_{core::logging::LoggerFactory<ZlibDecompressStream>::getLogger()} {
  int ret = inflateInit2(&strm_, 15 + (format == ZlibCompressionFormat::GZIP ? 16 : 0) /* windowBits */);
  if (ret != Z_OK) {
    logger_->log_error("Failed to initialize z_stream with inflateInit2, error code: {}", ret);
    throw Exception(ExceptionType::GENERAL_EXCEPTION, "zlib inflateInit2 failed");
  }

  state_ = ZlibStreamState::INITIALIZED;
}

ZlibDecompressStream::~ZlibDecompressStream() {
  if (state_ != ZlibStreamState::UNINITIALIZED) {
    int result = inflateEnd(&strm_);
    if (result == Z_STREAM_ERROR) {
      logger_->log_error("Stream state was inconsistent");
    } else if (result != Z_OK) {
      logger_->log_error("Unknown error while finishing decompression {}", result);
    }
  }
}

size_t ZlibDecompressStream::write(const uint8_t* value, size_t size) {
  if (state_ != ZlibStreamState::INITIALIZED) {
    logger_->log_error("writeData called in invalid ZlibDecompressStream state, state is {}", magic_enum::enum_name(state_));
    return STREAM_ERROR;
  }

  strm_.next_in = const_cast<uint8_t*>(value);
  strm_.avail_in = gsl::narrow<uInt>(size);

  /*
   * inflate works similarly to deflate in that it will not leave input data unconsumed, and we have to watch avail_out,
   * but in this case we do not have to close the stream, because it will detect the end of the compressed format
   * and signal that it is ended by returning Z_STREAM_END and not accepting any more input data.
   */
  int ret;
  do {
    logger_->log_trace("writeData has {} B of input data left", strm_.avail_in);

    strm_.next_out = reinterpret_cast<Bytef*>(outputBuffer_.data());
    strm_.avail_out = gsl::narrow<uInt>(outputBuffer_.size());

    ret = inflate(&strm_, Z_NO_FLUSH);
    if (ret == Z_STREAM_ERROR ||
        ret == Z_NEED_DICT ||
        ret == Z_DATA_ERROR ||
        ret == Z_MEM_ERROR) {
      logger_->log_error("inflate failed, error code: {}", ret);
      state_ = ZlibStreamState::ERRORED;
      return STREAM_ERROR;
    }
    const auto output_size = outputBuffer_.size() - strm_.avail_out;
    logger_->log_trace("deflate produced {} B of output data", output_size);
    if (output_->write(gsl::make_span(outputBuffer_).subspan(0, output_size)) != output_size) {
      logger_->log_error("Failed to write to underlying stream");
      state_ = ZlibStreamState::ERRORED;
      return STREAM_ERROR;
    }
  } while (strm_.avail_out == 0);

  if (ret == Z_STREAM_END) {
    state_ = ZlibStreamState::FINISHED;
  }

  return size;
}

}  // namespace org::apache::nifi::minifi::io
