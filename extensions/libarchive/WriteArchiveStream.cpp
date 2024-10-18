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

#include "WriteArchiveStream.h"

#include <utility>
#include <string>

namespace org::apache::nifi::minifi::io {

processors::archive_write_unique_ptr WriteArchiveStreamImpl::createWriteArchive() const {
  processors::archive_write_unique_ptr arch{archive_write_new()};
  if (!arch) {
    logger_->log_error("Failed to create write archive");
    return nullptr;
  }

  int result = 0;

  result = archive_write_set_format_ustar(arch.get());
  if (result != ARCHIVE_OK) {
    logger_->log_error("Archive write set format ustar error {}", archive_error_string(arch.get()));
    return nullptr;
  }
  if (compress_format_ == CompressionFormat::GZIP) {
    result = archive_write_add_filter_gzip(arch.get());
    if (result != ARCHIVE_OK) {
      logger_->log_error("Archive write add filter gzip error {}", archive_error_string(arch.get()));
      return nullptr;
    }
    std::string option = "gzip:compression-level=" + std::to_string(compress_level_);
    result = archive_write_set_options(arch.get(), option.c_str());
    if (result != ARCHIVE_OK) {
      logger_->log_error("Archive write set options error {}", archive_error_string(arch.get()));
      return nullptr;
    }
  } else if (compress_format_ == CompressionFormat::BZIP2) {
    result = archive_write_add_filter_bzip2(arch.get());
    if (result != ARCHIVE_OK) {
      logger_->log_error("Archive write add filter bzip2 error {}", archive_error_string(arch.get()));
      return nullptr;
    }
  } else if (compress_format_ == CompressionFormat::LZMA) {
    result = archive_write_add_filter_lzma(arch.get());
    if (result != ARCHIVE_OK) {
      logger_->log_error("Archive write add filter lzma error {}", archive_error_string(arch.get()));
      return nullptr;
    }
  } else if (compress_format_ == CompressionFormat::XZ_LZMA2) {
    result = archive_write_add_filter_xz(arch.get());
    if (result != ARCHIVE_OK) {
      logger_->log_error("Archive write add filter xz error {}", archive_error_string(arch.get()));
      return nullptr;
    }
  } else {
    logger_->log_error("Archive write unsupported compression format");
    return nullptr;
  }
  result = archive_write_set_bytes_per_block(arch.get(), 0);
  if (result != ARCHIVE_OK) {
    logger_->log_error("Archive write set bytes per block error {}", archive_error_string(arch.get()));
    return nullptr;
  }
  result = archive_write_open(arch.get(), sink_.get(), nullptr, archive_write, nullptr);
  if (result != ARCHIVE_OK) {
    logger_->log_error("Archive write open error {}", archive_error_string(arch.get()));
    return nullptr;
  }
  return arch;
}

bool WriteArchiveStreamImpl::newEntry(const EntryInfo& info) {
  if (!arch_) {
    return false;
  }
  arch_entry_.reset(archive_entry_new());
  if (!arch_entry_) {
    logger_->log_error("Failed to create archive entry");
    return false;
  }
  archive_entry_set_pathname(arch_entry_.get(), info.filename.c_str());
  archive_entry_set_size(arch_entry_.get(), gsl::narrow<la_int64_t>(info.size));
  archive_entry_set_mode(arch_entry_.get(), S_IFREG | 0755);

  int result = archive_write_header(arch_.get(), arch_entry_.get());
  if (result != ARCHIVE_OK) {
    logger_->log_error("Archive write header error {}", archive_error_string(arch_.get()));
    return false;
  }
  return true;
}

size_t WriteArchiveStreamImpl::write(const uint8_t* data, size_t len) {
  if (!arch_ || !arch_entry_) {
    return STREAM_ERROR;
  }

  if (len == 0) {
    return 0;
  }
  gsl_Expects(data);

  int result = gsl::narrow<int>(archive_write_data(arch_.get(), data, len));
  if (result < 0) {
    logger_->log_error("Archive write data error {}", archive_error_string(arch_.get()));
    arch_entry_.reset();
    arch_.reset();
    return STREAM_ERROR;
  }

  return result;
}

bool WriteArchiveStreamImpl::finish() {
  if (!arch_) {
    return false;
  }
  arch_entry_.reset();
  // closing the archive is needed to complete the archive
  bool success = archive_write_close(arch_.get()) == ARCHIVE_OK;
  if (!success) {
    logger_->log_error("Archive write close error {}", archive_error_string(arch_.get()));
  }
  arch_.reset();
  return success;
}

WriteArchiveStreamImpl::~WriteArchiveStreamImpl() {
  WriteArchiveStreamImpl::finish();
}

}  // namespace org::apache::nifi::minifi::io
