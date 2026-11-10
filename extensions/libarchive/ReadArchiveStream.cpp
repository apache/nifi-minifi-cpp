// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "ReadArchiveStream.h"

namespace org::apache::nifi::minifi::io {

processors::archive_read_unique_ptr ReadArchiveStreamImpl::createReadArchive() {
  auto arch = processors::archive_read_unique_ptr{archive_read_new()};
  if (!arch) {
    logger_->log_error("Failed to create read archive");
    return nullptr;
  }

  int result = archive_read_support_format_all(arch.get());
  if (result != ARCHIVE_OK) {
    logger_->log_error("Archive read support format all error {}", archive_error_string(arch.get()));
    return nullptr;
  }
  result = archive_read_support_filter_all(arch.get());
  if (result != ARCHIVE_OK) {
    logger_->log_error("Archive read support filter all error {}", archive_error_string(arch.get()));
    return nullptr;
  }
  result = archive_read_open2(arch.get(), &reader_, nullptr, archive_read, nullptr, nullptr);
  if (result != ARCHIVE_OK) {
    logger_->log_error("Archive read open error {}", archive_error_string(arch.get()));
    return nullptr;
  }
  return arch;
}

std::optional<EntryInfo> ReadArchiveStreamImpl::nextEntry() {
  if (!arch_) {
    return std::nullopt;
  }
  entry_size_.reset();
  struct archive_entry *entry = nullptr;
  int result = archive_read_next_header(arch_.get(), &entry);
  if (result != ARCHIVE_OK) {
    if (result != ARCHIVE_EOF) {
      logger_->log_error("Archive read next header error {}", archive_error_string(arch_.get()));
    }
    return std::nullopt;
  }
  entry_size_ = gsl::narrow<size_t>(archive_entry_size(entry));
  logger_->log_debug("Archive entry size {}", entry_size_.value());
  return EntryInfo{archive_entry_pathname(entry), entry_size_.value()};
}

size_t ReadArchiveStreamImpl::read(const std::span<std::byte> out_buffer) {
  if (!arch_ || !entry_size_) {
    return STREAM_ERROR;
  }

  if (out_buffer.empty()) {
    return 0;
  }

  const la_ssize_t result = archive_read_data(arch_.get(), out_buffer.data(), out_buffer.size());
  if (result < 0) {
    logger_->log_error("Archive read data error {}", archive_error_string(arch_.get()));
    entry_size_.reset();
    arch_.reset();
    return STREAM_ERROR;
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::io
