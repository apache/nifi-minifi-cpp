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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/* ZlibBaseStream */

ZlibBaseStream::ZlibBaseStream()
    : ZlibBaseStream(this) {
}

ZlibBaseStream::ZlibBaseStream(DataStream* other)
    : BaseStream(other)
    , outputBuffer_(16384U) {
  strm_.zalloc = Z_NULL;
  strm_.zfree = Z_NULL;
  strm_.opaque = Z_NULL;
}

bool ZlibBaseStream::isFinished() const {
  return state_ == ZlibStreamState::FINISHED;
}

/* ZlibCompressStream */

ZlibCompressStream::ZlibCompressStream(ZlibCompressionFormat format, int level)
  : ZlibCompressStream(this, format, level) {
}

ZlibCompressStream::ZlibCompressStream(DataStream* other, ZlibCompressionFormat format, int level)
  : ZlibBaseStream(other) {
  int ret = deflateInit2(
      &strm_,
      level,
      Z_DEFLATED /* method */,
      15 + (format == ZlibCompressionFormat::GZIP ? 16 : 0) /* windowBits */,
      8 /* memLevel */,
      Z_DEFAULT_STRATEGY /* strategy */);
  if (ret != Z_OK) {
    logger_->log_error("Failed to initialize z_stream with deflateInit2, error code: %d", ret);
    throw Exception(ExceptionType::GENERAL_EXCEPTION, "zlib deflateInit2 failed");
  }

  state_ = ZlibStreamState::INITIALIZED;
}

ZlibCompressStream::~ZlibCompressStream() {
  if (state_ != ZlibStreamState::UNINITIALIZED) {
    deflateEnd(&strm_);
  }
}

int ZlibCompressStream::writeData(uint8_t* value, int size) {
  if (state_ != ZlibStreamState::INITIALIZED) {
    logger_->log_error("writeData called in invalid ZlibCompressStream state, state is %hhu", state_);
    return -1;
  }

  strm_.next_in = value;
  strm_.avail_in = size;

  /*
   * deflate consumes all input data it can (i.e. if it has enough output buffer it never leaves input data unconsumed)
   * and fills the output buffer to the brim every time it can. This means that the proper way to use deflate is to
   * watch avail_out: once deflate does not have to fill it to the brim, then it has consumed all data we have provided
   * to it, and does not need more output buffer for the time being.
   * When we have no more input data, we must call deflate with Z_FINISH to make it empty its internal buffers and
   * close the compressed stream.
   */
  do {
    logger_->log_trace("writeData has %u B of input data left", strm_.avail_in);

    strm_.next_out = outputBuffer_.data();
    strm_.avail_out = outputBuffer_.size();

    int flush = value == nullptr ? Z_FINISH : Z_NO_FLUSH;
    logger_->log_trace("calling deflate with flush %d", flush);

    int ret = deflate(&strm_, flush);
    if (ret == Z_STREAM_ERROR) {
      logger_->log_error("deflate failed, error code: %d", ret);
      state_ = ZlibStreamState::ERRORED;
      return -1;
    }
    int output_size = outputBuffer_.size() - strm_.avail_out;
    logger_->log_trace("deflate produced %d B of output data", output_size);
    if (BaseStream::writeData(outputBuffer_.data(), output_size) != output_size) {
      logger_->log_error("Failed to write to underlying stream");
      state_ = ZlibStreamState::ERRORED;
      return -1;
    }
  } while (strm_.avail_out == 0);

  return size;
}

void ZlibCompressStream::closeStream() {
  if (state_ == ZlibStreamState::INITIALIZED) {
    if (writeData(nullptr, 0U) == 0) {
      state_ = ZlibStreamState::FINISHED;
    }
  }
}

/* ZlibDecompressStream */

ZlibDecompressStream::ZlibDecompressStream(ZlibCompressionFormat format)
  : ZlibDecompressStream(this, format) {
}

ZlibDecompressStream::ZlibDecompressStream(DataStream* other, ZlibCompressionFormat format)
    : ZlibBaseStream(other) {
  int ret = inflateInit2(&strm_, 15 + (format == ZlibCompressionFormat::GZIP ? 16 : 0) /* windowBits */);
  if (ret != Z_OK) {
    logger_->log_error("Failed to initialize z_stream with inflateInit2, error code: %d", ret);
    throw Exception(ExceptionType::GENERAL_EXCEPTION, "zlib inflateInit2 failed");
  }

  state_ = ZlibStreamState::INITIALIZED;
}

ZlibDecompressStream::~ZlibDecompressStream() {
  if (state_ != ZlibStreamState::UNINITIALIZED) {
    inflateEnd(&strm_);
  }
}

int ZlibDecompressStream::writeData(uint8_t* value, int size) {
  if (state_ != ZlibStreamState::INITIALIZED) {
    logger_->log_error("writeData called in invalid ZlibDecompressStream state, state is %hhu", state_);
    return -1;
  }

  strm_.next_in = value;
  strm_.avail_in = size;

  /*
   * inflate works similarly to deflate in that it will not leave input data unconsumed, and we have to watch avail_out,
   * but in this case we do not have to close the stream, because it will detect the end of the compressed format
   * and signal that it is ended by returning Z_STREAM_END and not accepting any more input data.
   */
  int ret;
  do {
    logger_->log_trace("writeData has %u B of input data left", strm_.avail_in);

    strm_.next_out = outputBuffer_.data();
    strm_.avail_out = outputBuffer_.size();

    ret = inflate(&strm_, Z_NO_FLUSH);
    if (ret == Z_STREAM_ERROR ||
        ret == Z_NEED_DICT ||
        ret == Z_DATA_ERROR ||
        ret == Z_MEM_ERROR) {
      logger_->log_error("inflate failed, error code: %d", ret);
      state_ = ZlibStreamState::ERRORED;
      return -1;
    }
    int output_size = outputBuffer_.size() - strm_.avail_out;
    logger_->log_trace("deflate produced %d B of output data", output_size);
    if (BaseStream::writeData(outputBuffer_.data(), output_size) != output_size) {
      logger_->log_error("Failed to write to underlying stream");
      state_ = ZlibStreamState::ERRORED;
      return -1;
    }
  } while (strm_.avail_out == 0);

  if (ret == Z_STREAM_END) {
    state_ = ZlibStreamState::FINISHED;
  }

  return size;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
